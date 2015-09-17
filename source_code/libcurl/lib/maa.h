#ifndef HEADER_CURL_MAA_H
#define HEADER_CURL_MAA_H

#include <sys/time.h>

#define MAA_H2_SDK_VERSION   "1.2.0"
#define MAA_H2_HOST_4CNAME   "wsngh2.chinanetcenter.com"
#define MAA_H2_POST_URL      "http://collect.dsp.chinanetcenter.com/file"
#define MAA_H2_TCP_PORT      6480
#define MAA_H2_TLS_PORT      6443

enum curl_maa_type {
  CURL_MAATYPE_NOHTTP    = 0,/* Not HTTP protocol family */
  CURL_MAATYPE_H1_CNAME  = 1,/* CNAME result have not MAA host */
  CURL_MAATYPE_H1_PROXY  = 2,/* A CURLOPT_PROXY has been set */
  CURL_MAATYPE_H2        = 3,/* Via MAA proxy by http/2 */
  CURL_MAATYPE_LAST      = 7,
};

struct Curl_maa {
  int maa_port; /* Non-zero indicate that MAA force http/https port to be 
                  6480/6443, when DNS resolve returns a CNAME of MAA host. */
  enum curl_maa_type maa_type;
  bool https_switch;
  char cname[256];
};

void Curl_maa_on_url_complete(struct SessionHandle *data);
void Curl_maa_on_enable(struct SessionHandle *data, bool enable);
void Curl_maa_on_https_switch(struct SessionHandle *data, bool enable);
bool Curl_maa_is_https_switch(struct connectdata *conn);
void Curl_maa_on_connection_reuse(struct connectdata *conn);
void Curl_maa_check_connection_type(struct connectdata *conn);
bool Curl_maa_check_connection_reuseable(struct connectdata *needle,
                                         struct connectdata *check);
void Curl_maa_modify_remote_addr(struct connectdata *conn,
                                 struct sockaddr_in *addr);

#endif /* HEADER_CURL_MAA_H */
