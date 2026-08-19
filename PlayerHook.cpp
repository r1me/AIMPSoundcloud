#include "PlayerHook.h"
#include "AIMPSoundcloud.h"
#include <string>
#include "Tools.h"
#include "Config.h"

HRESULT WINAPI PlayerHook::OnCheckURL(IAIMPString *URL, BOOL *Handled) {
    if (!Plugin::instance()->isConnected())
        return E_FAIL;

    if (wcsstr(URL->GetData(), L"soundcloud://") == 0 && wcsstr(URL->GetData(), L"soundcloud.com") == 0)
        return E_FAIL;

    int64_t id = Tools::TrackIdFromUrl(URL->GetData());
    std::wstring stream_url = L"https://api.soundcloud.com/tracks/" + std::to_wstring(id) + L"/stream";
    
    if (auto ti = Tools::TrackInfo(id)) {
        stream_url = ti->Stream;
    }

    stream_url += L"\u000D\u000A" L"Authorization: OAuth " + Plugin::instance()->getAccessToken();

    URL->SetData(const_cast<wchar_t *>(stream_url.c_str()), stream_url.size());

    *Handled = 1;
    return S_OK;
}

HRESULT __stdcall WaveformProviderHook::Calculate(IAIMPString* FileURI, IAIMPTaskOwner* TaskOwner, PAIMPWaveformPeakInfo Peaks, INT32 PeakCount)
{
    if (!Plugin::instance()->isConnected())
        return E_FAIL;

    if (wcsstr(FileURI->GetData(), L"soundcloud://") == 0 && wcsstr(FileURI->GetData(), L"soundcloud.com") == 0)
        return E_FAIL;

    int64_t id = Tools::TrackIdFromUrl(FileURI->GetData());
    if (auto ti = Tools::TrackInfo(id)) {
    }

    return E_FAIL;
}
