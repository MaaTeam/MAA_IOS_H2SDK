#define _GNU_SOURCE //for getline() in stdio.h
#include "curl_setup.h"

#include <curl/curl.h>

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <netdb.h>

#include "urldata.h"
#include "multihandle.h"
#include "maa.h"
#include "maa_util.h"

struct curl_maa_handle{
  bool is_running;
  pthread_t thd;
  volatile bool thd_exit;
  volatile int log_line;
  curl_network_type nettype;
};

static pthread_mutex_t maa_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct curl_maa_handle maa_handle = {};
static struct curl_maa_config maa_conf = {
  .log_path = "",
  .log_max_record = 100,
  .log_per_report = 10,
};

/* cname to compare */
static char maa_cname[256];

static const char* http_method_str[HTTPREQ_LAST] = {
  "NONE",  /* HTTPREQ_NONE */
  "GET",   /* HTTPREQ_GET */
  "POST",  /* HTTPREQ_POST */
  "POST",  /* HTTPREQ_POST_FORM */
  "PUT",   /* HTTPREQ_PUT */
  "HEAD",  /* HTTPREQ_HEAD */
  "CUSTOM",/* HTTPREQ_CUSTOM */
};

static const char* http_proxy_type_str[CURL_MAATYPE_LAST] = {
  "NONE",   /* CURL_MAATYPE_NOHTTP, no used */
  "D",      /* CURL_MAATYPE_H1_CNAME */
  "DP",     /* CURL_MAATYPE_H1_PROXY */
  "P",      /* CURL_MAATYPE_H2 */
};

static const char* https_proxy_type_str[CURL_MAATYPE_LAST] ={
  "NONE",   /* CURL_MAATYPE_NOHTTP, no used */
  "DS",     /* CURL_MAATYPE_H1_CNAME */
  "DPS",    /* CURL_MAATYPE_H1_PROXY */
  "PS",     /* CURL_MAATYPE_H2 */
};

static const char* network_type_str[CURL_NETWORK_LAST] = {
  "NONE",  /* CURL_NETWORK_NONE */
  "2G",    /* CURL_NETWORK_2G */
  "3G",    /* CURL_NETWORK_3G */
  "4G",    /* CURL_NETWORK_4G */
  "WIFI",  /* CURL_NETWORK_WIFI */
};

extern const struct Curl_handler Curl_handler_https;

static const char* http_ver_str(int version)
{
  if (version == 10) {
    return "http/1.0";
  } else if (version == 20) {
    return "http/2.0";
  } else if (version == 11) {
    return "http/1.1";
  } else {
    return "http/0.0";
  }
}

static void maa_post_log_by_http(char* buf, size_t len)
{
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    return;
  }

  struct timeval tv;
  char timestamp[16];

  gettimeofday(&tv, NULL);
  snprintf(timestamp, 16, "%ld", tv.tv_sec + tv.tv_usec / 1000);

  static int xxtea_encoded = 0;
  static uint32_t imei_len = 0, imsi_len = 0;
  if (xxtea_encoded == 0) {
    maa_util_xxtea_encode((unsigned char *)maa_conf.imei, strlen(maa_conf.imei), &imei_len);
    maa_util_xxtea_encode((unsigned char *)maa_conf.imsi, strlen(maa_conf.imsi), &imsi_len);
    xxtea_encoded = 1;
  }

  struct curl_httppost *post, *last;
  post = last = NULL;

  curl_formadd(&post, &last, CURLFORM_COPYNAME, "date", CURLFORM_PTRCONTENTS,
               timestamp, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "maaid", CURLFORM_PTRCONTENTS,
               maa_conf.imei, CURLFORM_CONTENTSLENGTH, (long)imei_len, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "maasi", CURLFORM_PTRCONTENTS,
               maa_conf.imsi, CURLFORM_CONTENTSLENGTH, (long)imsi_len, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "packageName", CURLFORM_PTRCONTENTS,
               maa_conf.package_name, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "platform", CURLFORM_PTRCONTENTS,
               maa_conf.platform, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "appVersion", CURLFORM_PTRCONTENTS,
               maa_conf.app_version, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "type", CURLFORM_PTRCONTENTS, "h2sdk", CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "sdkVersion", CURLFORM_PTRCONTENTS,
               MAA_H2_SDK_VERSION, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "networkType", CURLFORM_PTRCONTENTS,
               network_type_str[maa_handle.nettype], CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "codec", CURLFORM_PTRCONTENTS, "gzip", CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "filename", CURLFORM_BUFFER, "h2accesslog.gzip",
               CURLFORM_BUFFERPTR, buf, CURLFORM_BUFFERLENGTH, len, CURLFORM_END);

  curl_easy_setopt(curl, CURLOPT_URL, MAA_H2_POST_URL);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); /* timeout */

  curl_easy_perform(curl);

  curl_formfree(post);
  curl_easy_cleanup(curl);
}

static int maa_report_log(FILE* fp, int line)
{
  const size_t LOG_BUF_LEN = 5 * 1024 * maa_conf.log_per_report;/* 5k per line */
  char *log_buf = calloc(1, LOG_BUF_LEN);
  size_t log_buf_len = 0;
  char *line_ptr = NULL;
  size_t line_len = 0;
  ssize_t read;
  int n = 0;

  if (log_buf == NULL) {
    return line;
  }

  pthread_mutex_lock(&maa_mutex);

  while ((read = getline(&line_ptr, &line_len, fp)) != -1) {
    if (log_buf_len + read >= LOG_BUF_LEN || n >= maa_conf.log_per_report)
      break;
    strcat(log_buf, line_ptr);
    log_buf_len += read;
    line++;
    n++;
  }
  {
    /* Reset the read position, otherwise next call of getline() will always return -1. (for MAC OS) */
    ssize_t last_pos = ftell(fp);
    fseek(fp, last_pos, SEEK_SET);
  }
  if (line_ptr) {
    free(line_ptr);
  }

  pthread_mutex_unlock(&maa_mutex);

  if (log_buf_len) {
    size_t log_buf_gz_len = LOG_BUF_LEN;
    char *log_buf_gz = calloc(1, LOG_BUF_LEN);
    if (log_buf_gz) {
      if (maa_util_gzcompress((Bytef *)log_buf_gz, (uLongf *)&log_buf_gz_len, 
          (Bytef *)log_buf, (uLong)log_buf_len) == 0) {
        maa_post_log_by_http(log_buf_gz, log_buf_gz_len);
      }
      free(log_buf_gz);
    }
  }
  free(log_buf);

  return line;
}

static void maa_dump_log(struct SessionHandle *data)
{
#define LMS(x) (long)((x) * 1000.0)
  struct Progress* p = &(data->progress);
  struct timeval tv = {};

  const char** proxy_type_str;
  if (data->maa.maa_port == MAA_H2_TCP_PORT) {
    proxy_type_str = http_proxy_type_str;
  } else if (data->maa.maa_port == MAA_H2_TLS_PORT) {
    proxy_type_str = https_proxy_type_str;
  } else {
    if (strstr(data->set.str[STRING_SET_URL], "http://") != NULL) {
      proxy_type_str = http_proxy_type_str;
    } else {
      proxy_type_str = https_proxy_type_str;
    }
  }

  gettimeofday(&tv, NULL);
  FILE *fp = fopen(maa_conf.log_path, "a");
  if (fp == NULL) {
    return;
  }

  static const char* MAA_LOG_FORMAT_PART0 = {
    "%ld\t"  /* Timestamp(Millisecond) */
    "%s\t"   /* Via Proxy */
    "%s\t"   /* Method */
    "%s\t"   /* URL */
    "%s\t"   /* Content Type */
    "%d\t"   /* Status Code */
    "%s\t"   /* Protocol */
    "%ld\t"  /* DNS time */
    "%ld\t"  /* Connect time */
    "%ld\t"  /* Send time */
    "%ld\t"  /* Wait time */
    "%ld\t"  /* Receive time */
    "%s\t"   /* Host Address */
    "%ld\t"  /* Content-Length */
  };

  static const char* MAA_LOG_FORMAT_PART1 = {
    "%s"     /* First CNAME */
    /* Fill in new field below here */
    "\r\n"   /* CRLF */
  };

  pthread_mutex_lock(&maa_mutex);

  fprintf(fp, MAA_LOG_FORMAT_PART0,
    tv.tv_sec + tv.tv_usec / 1000,             /* Timestamp(Millisecond) */
    proxy_type_str[data->maa.maa_type],        /* Via Proxy */
    http_method_str[data->set.httpreq],        /* Method */
    data->set.str[STRING_SET_URL],             /* URL */
    data->info.contenttype,                    /* Content Type */
    data->info.httpcode,                       /* Status Code */
    http_ver_str(data->info.httpversion),      /* Protocol */
    LMS(p->t_nslookup),                        /* DNS time */
    LMS(p->t_connect - p->t_nslookup),         /* Connect time */
    LMS(p->t_pretransfer - p->t_connect),      /* Send time */
    LMS(p->t_starttransfer - p->t_pretransfer),/* Wait time */
    LMS(p->timespent - p->t_starttransfer),    /* Receive time */
    data->info.conn_primary_ip,                /* Host Address */
    data->progress.size_dl                     /* Content-Length */
    );
  /* should fprintf cname in another call, otherwise '(null)' will be fprintf instead. */
  if (strlen(data->maa.cname)) {
    fprintf(fp, MAA_LOG_FORMAT_PART1, data->maa.cname); /* First CNAME */
  } else {
    fprintf(fp, MAA_LOG_FORMAT_PART1, "null");          /* First CNAME */
  }
  fflush(fp);

  pthread_mutex_unlock(&maa_mutex);

  fclose(fp);

  maa_handle.log_line++;
}

static void* maa_thread_handler(void *ctx)
{
  int line_num = 0;/* number of reported log lines */
  FILE* fp = fopen(maa_conf.log_path, "r");
  if (fp == NULL) {
    pthread_exit(NULL);
  }

  while(1) {
    if (maa_handle.thd_exit ||
        line_num >= maa_conf.log_max_record ||
        access(maa_conf.log_path, F_OK) != 0) {
      fclose(fp);
      pthread_exit(NULL);
    }

    /* check per second and report at least "log_per_report" pieces */
    if (maa_handle.log_line - line_num >= maa_conf.log_per_report) {
      line_num = maa_report_log(fp, line_num);
      assert(line_num <= maa_handle.log_line);
    }

    sleep(1);
  }

  pthread_exit(NULL);
}

static void* maa_cname_update_thread_handler(void *ctx)
{
  struct hostent *host;

  if ((host = gethostbyname(MAA_H2_HOST_4CNAME)) == NULL) {
    pthread_exit(NULL);
  }

  if (strlen(host->h_name) < sizeof(maa_cname)) {
    strncpy(maa_cname, host->h_name, strlen(host->h_name));
  }

  pthread_exit(NULL);
}

CURLcode curl_maa_global_init(struct curl_maa_config *conf)
{
#ifndef USE_NGHTTP2
  return CURLE_HTTP2; /* MAA is not available without http2 */
#endif

  if (conf == NULL || conf->log_path[0] == '\0') {
    return CURLE_FAILED_INIT;
  }

  bzero(maa_cname, sizeof(maa_cname));

  pthread_t ign_thd;
  /* init maa cname update thread */
  if (pthread_create(&ign_thd, NULL, maa_cname_update_thread_handler, NULL) < 0) {
    return CURLE_FAILED_INIT;
  }
  pthread_detach(ign_thd);

  /* init data */
  bzero(&maa_handle, sizeof(struct curl_maa_handle));
  memcpy(&maa_conf, conf, sizeof(struct curl_maa_config));

  /* init file */
  FILE* fp = fopen(maa_conf.log_path, "w");
  if (fp == NULL) {
    goto err_exit;
  }
  fclose(fp);

  /* init thread and mutex*/
  if (pthread_create(&maa_handle.thd, NULL, maa_thread_handler, NULL) < 0) {
    goto err_exit;
  }

  /* all OK */
  maa_handle.is_running = true;
  return CURLE_OK;

err_exit:
  curl_maa_global_cleanup();
  return CURLE_FAILED_INIT;
}

void curl_maa_global_cleanup(void)
{
  if (maa_handle.thd) {
    maa_handle.thd_exit = true;
    pthread_join(maa_handle.thd, NULL);
  }

  /*is_running = false*/
  memset(&maa_handle, 0, sizeof(struct curl_maa_handle));
}

void curl_maa_on_network_type_changed(curl_network_type type)
{
  maa_handle.nettype = type;
}

void Curl_maa_on_url_complete(struct SessionHandle *data)
{
  if (!maa_handle.is_running || data == NULL || !data->maa_enable
      || data->maa.maa_type == CURL_MAATYPE_NOHTTP
      || maa_handle.log_line >= maa_conf.log_max_record
      || access(maa_conf.log_path, F_OK) != 0) {
    return;
  }

  maa_dump_log(data);
}

void Curl_maa_check_connection_type(struct connectdata* conn)
{
  struct SessionHandle *data = conn->data;

  conn->maa.maa_port = 0;
  conn->maa.maa_type = CURL_MAATYPE_NOHTTP;
  conn->maa.https_switch = false;

  if (conn == NULL || data == NULL || conn->handler == NULL
      || conn->dns_entry == NULL || conn->dns_entry->addr == NULL
      || !data->maa_enable)
    return;

  if (!maa_handle.is_running) {
    data->maa_enable = false;
    return;
  }

  if (conn->proxy.name != NULL && conn->proxy.name[0] != '\0') {
    conn->maa.maa_type = CURL_MAATYPE_H1_PROXY;
    goto type_exit;
  }

  const char* cname = conn->dns_entry->addr->ai_canonname;
  if (cname == NULL || strlen(maa_cname) == 0
      || strstr(cname, maa_cname) == NULL) {
    conn->maa.maa_type = CURL_MAATYPE_H1_CNAME;
    goto type_exit;
  }

  conn->maa.maa_type = CURL_MAATYPE_H2;
  strncpy(conn->maa.cname,
          conn->dns_entry->addr->ai_canonname,
          sizeof(conn->maa.cname));

  data->set.httpversion = CURL_HTTP_VERSION_2_0;

  if ((conn->handler->protocol & CURLPROTO_HTTP)
      && data->maa_https_switch) {
    conn->handler = conn->given = &Curl_handler_https;
    conn->maa.https_switch = true;
  }

  if (conn->handler->protocol & CURLPROTO_HTTP) {
    conn->maa.maa_port = MAA_H2_TCP_PORT;
  } else if (conn->handler->protocol & CURLPROTO_HTTPS) {
    conn->maa.maa_port = MAA_H2_TLS_PORT;
    conn->negnpn = CURL_HTTP_VERSION_2_0;
  } else { /* Unknown protocol */
    conn->maa.maa_type = CURL_MAATYPE_NOHTTP;
    conn->maa.maa_port = 0;
  }

type_exit:
  memcpy(&(data->maa), &(conn->maa), sizeof(struct Curl_maa));
}

bool Curl_maa_check_connection_reuseable(struct connectdata *needle, struct connectdata *check)
{
  if (needle == NULL || needle->data == NULL || check == NULL) {
    return false;
  }

  if (needle->data->maa_enable != (check->maa.maa_port != 0)) {
    return false;
  }

  if (needle->data->maa_https_switch != (check->maa.https_switch != false)) {
    return false;
  }

  return true;
}

void Curl_maa_on_enable(struct SessionHandle *data, bool enable)
{
  if (data == NULL) {
    return;
  }

  if (!maa_handle.is_running) {
    data->maa_enable = false;
    return;
  }

  data->maa_enable = enable;
}

void Curl_maa_on_https_switch(struct SessionHandle *data, bool enable)
{
  if (data == NULL) {
    return;
  }

  if (!maa_handle.is_running) {
    data->maa_https_switch = false;
    return;
  }

  data->maa_https_switch = enable;
}

bool Curl_maa_is_https_switch(struct connectdata* conn)
{
  if (conn == NULL) {
    return false;
  }

  return conn->maa.https_switch;
}

void Curl_maa_on_connection_reuse(struct connectdata* conn)
{
  if (conn == NULL || conn->data == NULL) {
    return;
  }

  memcpy(&(conn->data->maa), &(conn->maa), sizeof(struct Curl_maa));
}

void Curl_maa_modify_remote_addr(struct connectdata *conn, struct sockaddr_in *addr)
{
  if (conn == NULL || addr == NULL) {
    return;
  }

  if (conn->maa.maa_port != 0) {
    addr->sin_port = htons(conn->maa.maa_port);
  }
}

