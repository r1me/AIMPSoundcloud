#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <string>

#include "AimpHTTP.h"
#include "AIMPSoundcloud.h"
#include "AIMPString.h"
#include "Tools.h"
#include <process.h>
#include "SDK/apiFileManager.h"

bool AimpHTTP::m_initialized = false;
IAIMPServiceHTTPClient *AimpHTTP::m_httpClient = nullptr;
IAIMPServiceHTTPClient2 *AimpHTTP::m_httpClient2 = nullptr;

std::set<AimpHTTP::EventListener *> AimpHTTP::m_handlers;

AimpHTTP::EventListener::EventListener(CallbackFunc callback, bool isFile) : m_isFileStream(isFile), m_callback(callback) {
    AimpHTTP::m_handlers.insert(this);
}

AimpHTTP::EventListener::~EventListener() {
    AimpHTTP::m_handlers.erase(this);
}

void WINAPI AimpHTTP::EventListener::OnAccept(IAIMPString *ContentType, const INT64 ContentSize, BOOL *Allow) {
    ContentType->AddRef();
    ContentType->Release();
    *Allow = AimpHTTP::m_initialized && Plugin::instance()->core();
}

void WINAPI AimpHTTP::EventListener::OnAcceptHeaders(IAIMPString *Header, BOOL *Allow) {
    Header->AddRef();
    Header->Release();
    *Allow = AimpHTTP::m_initialized && Plugin::instance()->core();
}

void WINAPI AimpHTTP::EventListener::OnComplete(IAIMPErrorInfo *ErrorInfo, BOOL Canceled) {
    if (m_stream) {
        if (AimpHTTP::m_initialized && Plugin::instance()->core()) {
            if (m_isFileStream) {
                m_stream->Release();
                if (m_callback)
                    m_callback(nullptr, 0);
                return;
            }

            if (m_callback || m_imageContainer) {
                int s = (int)m_stream->GetSize();
                unsigned char *buf = new unsigned char[s + 1];
                buf[s] = 0;
                m_stream->Seek(0, AIMP_STREAM_SEEKMODE_FROM_BEGINNING);
                m_stream->Read(buf, s);

                if (m_callback) {
                    m_callback(buf, s);
                } else if (m_imageContainer) {
                    if (s <= m_maxSize) {
                        (*m_imageContainer)->SetDataSize(s);
                        memcpy((*m_imageContainer)->GetData(), buf, s);
                    } else {
                        (*m_imageContainer)->Release();
                        *m_imageContainer = nullptr;
                    }
                }
                delete[] buf;
            }
        }
        m_stream->Release();
    }
}

void WINAPI AimpHTTP::EventListener::OnProgress(const INT64 Downloaded, const INT64 Total) {

}

bool AimpHTTP::Get(const std::wstring &url, CallbackFunc callback, bool synchronous) {
    if (!AimpHTTP::m_initialized || !Plugin::instance()->core())
        return false;

    EventListener *listener = new EventListener(callback);
    Plugin::instance()->core()->CreateObject(IID_IAIMPMemoryStream, reinterpret_cast<void **>(&(listener->m_stream)));

    return SUCCEEDED(m_httpClient->Get(AIMPString(url), synchronous ? AIMP_SERVICE_HTTPCLIENT_FLAGS_WAITFOR : 0, listener->m_stream, listener, 0, reinterpret_cast<void **>(&(listener->m_taskId))));
}

bool AimpHTTP::Download(const std::wstring &url, const std::wstring &destination, CallbackFunc callback) {
    if (!AimpHTTP::m_initialized || !Plugin::instance()->core())
        return false;

    EventListener *listener = new EventListener(callback, true);
    IAIMPServiceFileStreaming *fileStreaming = nullptr;
    if (SUCCEEDED(Plugin::instance()->core()->QueryInterface(IID_IAIMPServiceFileStreaming, reinterpret_cast<void **>(&fileStreaming)))) {
        fileStreaming->CreateStreamForFile(AIMPString(destination), AIMP_SERVICE_FILESTREAMING_FLAG_CREATENEW, -1, -1, &(listener->m_stream));
        fileStreaming->Release();
    }
    
    return SUCCEEDED(m_httpClient->Get(AIMPString(url), 0, listener->m_stream, listener, 0, reinterpret_cast<void **>(&(listener->m_taskId))));
}

bool AimpHTTP::DownloadImage(const std::wstring &url, IAIMPImageContainer **Image, int maxSize) {
    if (!AimpHTTP::m_initialized || !Plugin::instance()->core())
        return false;

    EventListener *listener = new EventListener(nullptr);
    Plugin::instance()->core()->CreateObject(IID_IAIMPMemoryStream, reinterpret_cast<void **>(&(listener->m_stream)));

    if (SUCCEEDED(Plugin::instance()->core()->CreateObject(IID_IAIMPImageContainer, reinterpret_cast<void **>(Image)))) {
        listener->m_imageContainer = Image;
        listener->m_maxSize = maxSize;

        return SUCCEEDED(m_httpClient->Get(AIMPString(url), AIMP_SERVICE_HTTPCLIENT_FLAGS_WAITFOR, listener->m_stream, listener, 0, reinterpret_cast<void **>(&(listener->m_taskId))));
    }
    return false;
}

bool AimpHTTP::Post(const std::wstring &url, const std::string &body, CallbackFunc callback, bool synchronous) {
    if (!AimpHTTP::m_initialized || !Plugin::instance()->core())
        return false;

    IAIMPStream *postData = nullptr;
    if (SUCCEEDED(Plugin::instance()->core()->CreateObject(IID_IAIMPMemoryStream, reinterpret_cast<void **>(&postData)))) {
        postData->Write((unsigned char *)(body.data()), body.size(), nullptr);

        EventListener *listener = new EventListener(callback);
        Plugin::instance()->core()->CreateObject(IID_IAIMPMemoryStream, reinterpret_cast<void **>(&(listener->m_stream)));

        bool ok = SUCCEEDED(m_httpClient->Post(AIMPString(url), synchronous ? AIMP_SERVICE_HTTPCLIENT_FLAGS_WAITFOR : 0, listener->m_stream, postData, listener, 0, reinterpret_cast<void **>(&(listener->m_taskId))));
        postData->Release();
        return ok;
    }
    return false;
}

bool AimpHTTP::Put(const std::wstring& url, CallbackFunc callback) {
    if (!AimpHTTP::m_initialized || !Plugin::instance()->core())
        return false;

    EventListener* listener = new EventListener(callback);
    Plugin::instance()->core()->CreateObject(IID_IAIMPMemoryStream, reinterpret_cast<void**>(&(listener->m_stream)));

    return SUCCEEDED(m_httpClient2->Post(AIMPString(url), AIMP_SERVICE_HTTPCLIENT_METHOD_PUT, 0, listener->m_stream, NULL, listener, 0, reinterpret_cast<void**>(&(listener->m_taskId))));
}

bool AimpHTTP::Delete(const std::wstring& url, CallbackFunc callback) {
    if (!AimpHTTP::m_initialized || !Plugin::instance()->core())
        return false;

    EventListener* listener = new EventListener(callback);
    Plugin::instance()->core()->CreateObject(IID_IAIMPMemoryStream, reinterpret_cast<void**>(&(listener->m_stream)));

    return SUCCEEDED(m_httpClient2->Post(AIMPString(url), AIMP_SERVICE_HTTPCLIENT_METHOD_DELETE, 0, listener->m_stream, NULL, listener, 0, reinterpret_cast<void**>(&(listener->m_taskId))));
}

bool AimpHTTP::Init(IAIMPCore *Core) {
    m_initialized = SUCCEEDED(Core->QueryInterface(IID_IAIMPServiceHTTPClient, reinterpret_cast<void **>(&m_httpClient)));
    Core->QueryInterface(IID_IAIMPServiceHTTPClient2, reinterpret_cast<void**>(&m_httpClient2));

    return m_initialized;
}

void AimpHTTP::Deinit() {
    m_initialized = false;

    std::unordered_set<uintptr_t *> ids;
    for (auto x : m_handlers) ids.insert(x->m_taskId);
    for (auto x : ids) m_httpClient->Cancel(x, AIMP_SERVICE_HTTPCLIENT_FLAGS_WAITFOR);
    for (auto x : ids) m_httpClient2->Cancel(x, AIMP_SERVICE_HTTPCLIENT_FLAGS_WAITFOR);

    if (m_httpClient) {
        m_httpClient->Release();
        m_httpClient = nullptr;
    }

    if (m_httpClient2) {
        m_httpClient2->Release();
        m_httpClient2 = nullptr;
    }
}
