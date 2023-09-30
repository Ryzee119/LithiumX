#include "lithiumx.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/error.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/platform.h"
#include <lwip/netdb.h>
#include <lwip/sockets.h>

#define _CRT_RAND_S
#include <stdlib.h>

static char *content_buffer;
static int mbedtls_entropy(void *data, unsigned char *output, size_t len, size_t *olen);
static int mbedtls_net_send(void *ctx, const unsigned char *buf, size_t len);
static int mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len);
static int mbedtls_net_connect(mbedtls_net_context *ctx, const char *host,
                               const char *port, int proto);
static void my_debug( void *ctx, int level, const char *file, int line, const char *str);
static void *read_crt(int *len);
static int read_header(mbedtls_ssl_context *ssl);
//MBEDTLS_SSL_IN_CONTENT_LEN

//1. GET /repos/Ryzee119/LithiumX/releases/latest, check "assets" -> "name" and "created at", copy "url"
//2. GET /repos/Ryzee119/LithiumX/releases/assets/113337012 with "Accept: application/octet-stream\r\n", copy "Location" in header
//3. GET from objects.githubusercontent.com at "Location". This actually downloads the file

#if (1)
#define GITHUB_API_HOST "api.github.com"
#define GITHUB_API_PORT "443"
#define HTTP_REQUEST "GET /repos/Ryzee119/LithiumX/releases/assets/113337012 HTTP/1.1\r\n" \
                     "Host: " GITHUB_API_HOST "\r\n"                                       \
                     "Accept: application/octet-stream\r\n"                                \
                     "Accept-Encoding: none\r\n"                                           \
                     "X-GitHub-Api-Version: 2022-11-28\r\n"                                \
                     "User-Agent: curl\r\n\r\n"
#else
#define GITHUB_API_HOST "api.github.com"
#define GITHUB_API_PORT "443"
#define HTTP_REQUEST "GET /repos/Ryzee119/LithiumX/releases/latest HTTP/1.1\r\n" \
                     "Host: " GITHUB_API_HOST "\r\n"                             \
                     "Accept: application/vnd.github+json\r\n"                   \
                     "Accept-Encoding: none\r\n"                                 \
                     "X-GitHub-Api-Version: 2022-11-28\r\n"                      \
                     "User-Agent: curl\r\n\r\n"
#endif

#if (0)
#define GITHUB_API_HOST "objects.githubusercontent.com"
#define GITHUB_API_PORT "443"
#define HTTP_REQUEST "GET /github-production-release-asset-2e65be/..... HTTP/1.1\r\n" \
                     "Host: " GITHUB_API_HOST "\r\n"                             \
                     "Accept: application/vnd.github+json\r\n"                   \
                     "Accept-Encoding: none\r\n"                                 \
                     "X-GitHub-Api-Version: 2022-11-28\r\n"                      \
                     "User-Agent: curl\r\n\r\n"
#endif

static const char cert[] =
    "# Issuer: CN=DigiCert Global Root CA O=DigiCert Inc OU=www.digicert.com\r\n"
    "# Subject: CN=DigiCert Global Root CA O=DigiCert Inc OU=www.digicert.com\r\n"
    "# Label: DigiCert Global Root CA\r\n"
    "# Serial: 10944719598952040374951832963794454346\r\n"
    "# MD5 Fingerprint: 79:e4:a9:84:0d:7d:3a:96:d7:c0:4f:e2:43:4c:89:2e\r\n"
    "# SHA1 Fingerprint: a8:98:5d:3a:65:e5:e5:c4:b2:d7:d6:6d:40:c6:dd:2f:b1:9c:54:36\r\n"
    "# SHA256 Fingerprint: 43:48:a0:e9:44:4c:78:cb:26:5e:05:8d:5e:89:44:b4:d8:4f:96:62:bd:26:db:25:7f:89:34:a4:43:c7:01:61\r\n"
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\r\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\r\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\r\n"
    "QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\r\n"
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\r\n"
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\r\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\r\n"
    "CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\r\n"
    "nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\r\n"
    "43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\r\n"
    "T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\r\n"
    "gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\r\n"
    "BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\r\n"
    "TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\r\n"
    "DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\r\n"
    "hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\r\n"
    "06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\r\n"
    "PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\r\n"
    "YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\r\n"
    "CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\r\n"
    "-----END CERTIFICATE-----";

void github_check(void *param)
{
    content_buffer = lv_mem_alloc(MBEDTLS_SSL_IN_CONTENT_LEN);
    if (content_buffer == NULL)
    {
        return;
    }

    mbedtls_net_context server_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;

    mbedtls_debug_set_threshold(1);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    mbedtls_entropy_add_source(&entropy, mbedtls_entropy, NULL, 0, MBEDTLS_ENTROPY_SOURCE_STRONG);

    if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0) != 0)
    {
        DbgPrint("DRBG seeding failed\n");
        return;
    }

    if (mbedtls_x509_crt_parse(&cacert, (const unsigned char *)cert, sizeof(cert)) != 0)
    {
        DbgPrint("Could not load certificates\n");
        return;
    }

    if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0)
    {
        DbgPrint("TLS configuration failed\n");
        return;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);
    mbedtls_ssl_set_hostname(&ssl, GITHUB_API_HOST);
    mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    if (mbedtls_net_connect(&server_fd, GITHUB_API_HOST, GITHUB_API_PORT, MBEDTLS_NET_PROTO_TCP) != 0)
    {
        DbgPrint("Failed to connect to server\n");
        return;
    }

    if (mbedtls_ssl_setup(&ssl, &conf) != 0)
    {
        DbgPrint("SSL setup failed\n");
        return;
    }

    while (1)
    {
        int res = mbedtls_ssl_handshake(&ssl);
        if (res == 0 || res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            if (res == 0) break;
        }
        else
        {
            return;
        }
    }

    if (mbedtls_ssl_write(&ssl, (const unsigned char *)HTTP_REQUEST, strlen(HTTP_REQUEST)) <= 0)
    {
        DbgPrint("Failed to send HTTP request\n");
        return;
    }

    int remaining_len = read_header(&ssl);
    while (remaining_len > 0)
    {
        int len = mbedtls_ssl_read(&ssl, (unsigned char *)content_buffer, MBEDTLS_SSL_IN_CONTENT_LEN - 1);
        if (len < 0) break;
        for (int k = 0; k < len; k++)
        {
            DbgPrint("%c", content_buffer[k]);
        }
        remaining_len -= len;
    }

    mbedtls_x509_crt_free(&cacert);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);

    lv_mem_free(content_buffer);
    return;
}

static int mbedtls_entropy(void *data, unsigned char *output, size_t len, size_t *olen)
{
    if (len < 4)
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;

    size_t written = 0;
    while (written < len - 4)
    {
        rand_s((unsigned int *)output);
        output += 4;
        written += 4;
    }

    *olen = written;
    return 0;
}

static int mbedtls_net_send(void *ctx, const unsigned char *buf, size_t len)
{
    int fd = ((mbedtls_net_context *)ctx)->fd;
    return send(fd, buf, len, 0);
}

static int mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len)
{
    int fd = ((mbedtls_net_context *)ctx)->fd;
    return recv(fd, buf, len, 0);
}

static int mbedtls_net_connect(mbedtls_net_context *ctx, const char *host,
                               const char *port, int proto)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    struct addrinfo hints, *addr_list, *cur;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = proto == MBEDTLS_NET_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = proto == MBEDTLS_NET_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP;

    if (getaddrinfo(host, port, &hints, &addr_list) != 0)
        return (MBEDTLS_ERR_NET_UNKNOWN_HOST);

    ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next)
    {
        ctx->fd = (int)socket(cur->ai_family, cur->ai_socktype,
                              cur->ai_protocol);
        if (ctx->fd < 0)
        {
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        if (connect(ctx->fd, cur->ai_addr, cur->ai_addrlen) == 0)
        {
            ret = 0;
            break;
        }

        close(ctx->fd);
        ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
    }

    freeaddrinfo(addr_list);

    return (ret);
}

static void my_debug( void *ctx, int level,
                      const char *file, int line,
                      const char *str )
{
    (void) level;
    DbgPrint("%s:%d: %s\n", file, line, str );
}

static void *read_crt(int *len)
{
    FILE *file;
    char *buffer;
    long file_length;

    file = fopen("Q:\\ca-certificates.crt", "rb");
    if (file == NULL) return NULL;

    fseek(file, 0, SEEK_END);
    file_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = (char *)lv_mem_alloc(file_length) + 1;
    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }
    fread(buffer, 1, file_length, file);
    buffer[file_length] = '\0';

    if (strstr((const char *)buffer, "-----BEGIN ") != NULL)
        file_length++;

    fclose(file);
    *len = file_length;
    return buffer;
}

static int read_header(mbedtls_ssl_context *ssl)
{
    unsigned char read_buf[2];
    int packet_len = 0, index = 0, cnt = 0;

    do
    {
        read_buf[1] = 0;
        if (mbedtls_ssl_read(ssl, read_buf, 1) == 0) return 0;

        content_buffer[index++] = read_buf[0];

        if (cnt == 0) {
            if(read_buf[0] == '\r') cnt++; else cnt=0;
            continue;
        }
        if (cnt == 1) {
            if(read_buf[0] == '\n') cnt++; else cnt=0;
            continue;
        }
        if (cnt == 2) {
            if(read_buf[0] == '\r') cnt++; else cnt=0;
            continue;
        }
        if (cnt == 3) {
            if(read_buf[0] == '\n') cnt++; else cnt=0;
            break;
        }
    } while (1);
    content_buffer[index] = '\0';

    for (int k = 0; k < index; k++)
    {
        DbgPrint("%c", content_buffer[k]);
    }

    char *content_len = strstr(content_buffer, "Content-Length:");
    if (content_len)
    {
        sscanf(content_len, "Content-Length: %d", &packet_len);
    }
    DbgPrint("CONTENT LENGTH %d\n\n\n\n\n\n", packet_len);
    return packet_len;
}
