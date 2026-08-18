#include "../include/common/email.h"
#include "../include/common/config.h"
#include <curl/curl.h>
#include <cstdio>
#include <cstring>

namespace chat {

struct UploadStatus {
    const char* data;
    size_t pos;
    size_t size;
};

static size_t read_callback(char* ptr, size_t size, size_t nmemb, void* userp) {
    UploadStatus* status = (UploadStatus*)userp;
    size_t remaining = status->size - status->pos;
    size_t to_copy = size * nmemb;
    if (to_copy > remaining) to_copy = remaining;
    if (to_copy == 0) return 0;
    memcpy(ptr, status->data + status->pos, to_copy);
    status->pos += to_copy;
    return to_copy;
}

bool EmailSender::send_code(const std::string& to, const std::string& code) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string from = Config::get("smtp.user");
    std::string password = Config::get("smtp.password");
    std::string body = "From: " + from + "\r\n"
                       "To: " + to + "\r\n"
                       "Subject: 聊天室验证码\r\n"
                       "Content-Type: text/plain; charset=UTF-8\r\n"
                       "\r\n"
                       "您的验证码是：" + code + "\r\n"
                       "5分钟内有效，请勿泄露。\r\n";

    UploadStatus status;
    status.data = body.c_str();
    status.pos = 0;
    status.size = body.size();

    struct curl_slist* recipients = NULL;
    recipients = curl_slist_append(recipients, to.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.qq.com:465");
    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_USERNAME, from.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_MAIL_AUTH, from.c_str());
    curl_easy_setopt(curl, CURLOPT_LOGIN_OPTIONS, "AUTH=LOGIN");
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &status);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    return res == CURLE_OK;
}

} // namespace chat