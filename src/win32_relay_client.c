#include "printdrop/win32_relay_client.h"

#ifdef _WIN32

#include <curl/curl.h>

#include <string.h>

static bool pd_relay_url_is_wss(const char *url)
{
    static const char prefix[] = "wss://";
    size_t length;

    if (url == NULL) {
        return false;
    }

    length = strlen(url);
    return length > sizeof(prefix) - 1U &&
           length <= (size_t)PD_RELAY_URL_MAX_BYTES &&
           memcmp(url, prefix, sizeof(prefix) - 1U) == 0;
}

bool pd_win32_curl_global_init(void)
{
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}

void pd_win32_curl_global_cleanup(void)
{
    curl_global_cleanup();
}

pd_relay_client_result pd_win32_relay_client_init(pd_win32_relay_client *client,
                                                   const char *wss_url)
{
    CURL *easy;
    CURLcode code;
    size_t url_length;

    if (client == NULL || wss_url == NULL) {
        return PD_RELAY_CLIENT_INVALID_ARGUMENT;
    }

    memset(client, 0, sizeof(*client));

    if (!pd_relay_url_is_wss(wss_url)) {
        return PD_RELAY_CLIENT_INVALID_URL;
    }

    easy = curl_easy_init();
    if (easy == NULL) {
        client->state = PD_RELAY_CLIENT_FAILED;
        return PD_RELAY_CLIENT_CURL_ERROR;
    }

    code = curl_easy_setopt(easy, CURLOPT_URL, wss_url);
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_CONNECT_ONLY, 2L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 2L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L);
    }

    if (code != CURLE_OK) {
        curl_easy_cleanup(easy);
        client->state = PD_RELAY_CLIENT_FAILED;
        return PD_RELAY_CLIENT_CURL_ERROR;
    }

    url_length = strlen(wss_url);
    memcpy(client->url, wss_url, url_length + 1U);
    client->easy_handle = easy;
    client->state = PD_RELAY_CLIENT_PREPARED;
    return PD_RELAY_CLIENT_OK;
}

void pd_win32_relay_client_cleanup(pd_win32_relay_client *client)
{
    if (client == NULL) {
        return;
    }

    if (client->easy_handle != NULL) {
        curl_easy_cleanup((CURL *)client->easy_handle);
        client->easy_handle = NULL;
    }
    memset(client->url, 0, sizeof(client->url));
    client->state = PD_RELAY_CLIENT_CLOSED;
}

const char *pd_win32_relay_client_curl_version(void)
{
    const curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    return info == NULL ? NULL : info->version;
}

const char *pd_win32_relay_client_ssl_backend(void)
{
    const curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    return info == NULL ? NULL : info->ssl_version;
}

#endif
