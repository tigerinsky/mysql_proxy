 
/**
 * @file mysql_proxy.h
 * @author way
 * @date 2015/02/01 16:25:46
 * @brief 
 *  
 **/

#ifndef  __MYSQL_PROXY_H_
#define  __MYSQL_PROXY_H_

#include <stdint.h>
#include <string>
#include <vector>
#include "mysql.h"

namespace google {
namespace protobuf{
class Message;
class Descriptor;
}
}

namespace tis {


struct mysql_stmt_t;
typedef int (*MAP_FUNC)(mysql_stmt_t* stmt, MYSQL_BIND* field, int number);

typedef struct proto_map_info_t {
    MAP_FUNC map_func;
    int number;
} proto_map_info_t;

typedef struct mysql_stmt_t {
    MYSQL_STMT* instance;
    MYSQL_BIND* arg;
    int* type;
    uint32_t* arg_off;
    uint32_t arg_num;
    MYSQL_BIND* field;
    proto_map_info_t* map_info;
    uint32_t field_num;
    char* bind_buff;
    uint32_t bind_buff_size;
    uint8_t* proto_buff;
    uint32_t proto_buff_size;
    uint32_t proto_size;
    std::string query;
    std::string para_buff;

    mysql_stmt_t() {
        instance = NULL; 
        arg = NULL;
        type = NULL;
        arg_off = NULL;
        arg_num = 0;
        field = NULL;
        map_info = NULL;
        field_num = 0;
        bind_buff = NULL;
        bind_buff_size = 0;
        proto_buff = NULL;
        proto_buff_size = 0;
        proto_size = 0;
    }
} mysql_stmt_t;

 

class MysqlProxy {
public:

    static const int PREPARE_BOOL = 0;
    static const int PREPARE_CHAR = 1;
    static const int PREPARE_INT16 = 2;
    static const int PREPARE_UINT16 = 3;
    static const int PREPARE_INT32 = 4;
    static const int PREPARE_UINT32 = 5;
    static const int PREPARE_INT64 = 6;
    static const int PREPARE_UINT64 = 7;
    static const int PREPARE_FLOAT = 8;
    static const int PREPARE_DOUBLE = 9;
    static const int PREPARE_STRING = 10;
    
    static const uint32_t DEFAULT_PROTO_BUFF_SIZE = 1;
    static const uint32_t DEFAULT_CONNECT_TIMEOUT = 3;
    static const uint32_t DEFAULT_READ_TIMEOUT = 3;
    static const uint32_t DEFAULT_WRITE_TIMEOUT = 3;
    static const uint32_t DEFAULT_PING_INTERVAL = 2;

    static const int MYSQL_CONNECT_OK = 0;
    static const int MYSQL_CONNECT_ERR = 1;

    static const int MYSQL_RECONN_OK = 0;
    static const int MYSQL_RECONN_MYSQL_ERR = 2;
    static const int MYSQL_RECONN_INIT_STMT_ERR = 1;

    static const int MYSQL_SELECT_DB_OK = 0;
    static const int MYSQL_SELECT_DB_ERR = 1;

    static const int MYSQL_SET_CHARSET_OK = 0;
    static const int MYSQL_SET_CHARSET_ERR = 1;

    static const int MYSQL_PREPARE_OK = 0;
    static const int MYSQL_PREPARE_INIT_ERR = 1;
    static const int MYSQL_PREPARE_PARSE_QUERY_ERR = 2;
    static const int MYSQL_PREPARE_CREATE_PARA_ERR = 3;
    static const int MYSQL_PREPARE_BIND_PARA_ERR = 4;
    static const int MYSQL_PREPARE_UNKNOWN_PARA_TYPE = 5;
    static const int MYSQL_PREPARE_UNSUPPORT_FIELD_TYPE = 6; 
    static const int MYSQL_PREPARE_BIND_RESULT_ERR = 7;
    static const int MYSQL_PREPARE_CREATE_MAP_FUNC_ERR = 8;
    static const int MYSQL_PREPARE_ORMTYPE_NOT_MATCH = 9;
    static const int MYSQL_PREPARE_CREATE_PROTO_BUFF_ERR = 10;
    static const int MYSQL_PREPARE_BIND_BUFF_ERR = 11;

    static const int MYSQL_QUERY_OK = 0;
    static const int MYSQL_QUERY_ERR = 1;
    static const int MYSQL_QUERY_RECONN_ERR = 2;
    static const int MYSQL_QUERY_FREE_RESULT_ERR = 3;

    static const int MYSQL_NEXT_OK = 0;
    static const int MYSQL_NEXT_END = 1;
    static const int MYSQL_NEXT_ERR = 2;

    static const int MYSQL_GET_PROTO_OK = 0;
    static const int MYSQL_GET_PROTO_ERR = 1;
    static const int MYSQL_GET_PROTO_BUF_MALLOC_FAIL = 2;
    static const int MYSQL_GET_PROTO_PARSE_ERR = 3;

public:
    static int library_init(void); 
    static void library_end(void); 

public:
    MysqlProxy();
    virtual ~MysqlProxy();

public:
    /******** getter && setter ********/
    void set_connect_timeout(uint32_t seconds) { _connect_timeout = seconds; }
    void set_read_timeout(uint32_t seconds) { _read_timeout = seconds; }
    void set_write_timeout(uint32_t seconds) { _write_timeout = seconds; }
    void set_ping_interval(uint32_t seconds) { _ping_interval = seconds < DEFAULT_PING_INTERVAL ? DEFAULT_PING_INTERVAL : seconds; }
    uint32_t get_connect_timeout(void) const { return _connect_timeout; }
    uint32_t get_read_timeout(void) const { return _read_timeout; }
    uint32_t get_write_timeout(void) const { return _write_timeout; }
    uint32_t get_ping_interval(void) const {return _ping_interval; }
    const char* get_host(void) const { return _host.c_str(); }
    uint32_t get_port(void) const { return _port; }
    const char* get_user(void) const { return _user.c_str(); }
    const char* get_passwd(void) const { return _passwd.c_str(); }
    const char* get_db(void) const { return _db.c_str(); }
    const char* get_charset() const { return _charset.c_str();}

public:
    int connect(const char* host, 
                uint32_t port, 
                const char* user, 
                const char* passwd);
    int reconnect();
    int select_db(const char* db);
    int set_charset(const char* name);
    int prepare(const char* query, 
                const ::google::protobuf::Descriptor* descriptor, 
                mysql_stmt_t* stmt, 
                ...);
    void free_prepare(mysql_stmt_t* stmt);
    int execute(mysql_stmt_t* stmt, ...);
    int free_result(mysql_stmt_t* stmt);
    void close(void);

    const char* get_err_msg(void);
    const char* get_prepare_err_msg(const mysql_stmt_t* stmt);
    uint64_t get_row_num(mysql_stmt_t* stmt);
    uint32_t get_field_num(mysql_stmt_t* stmt);
    uint64_t get_affected_row_num(mysql_stmt_t* stmt);
    uint64_t get_insert_id(mysql_stmt_t* stmt);

    int next(mysql_stmt_t* stmt);
    int get_proto(mysql_stmt_t* stmt, ::google::protobuf::Message* m);

private:
    int __connect();
    int __execute(mysql_stmt_t* stmt, va_list args);

    std::string _host;
    uint32_t _port;
    std::string _user;
    std::string _passwd;
    std::string _db;
    std::string _charset;

    uint32_t _connect_timeout;
    uint32_t _read_timeout;
    uint32_t _write_timeout;
    uint32_t _ping_interval;
    bool _has_connect;

    std::vector<mysql_stmt_t*> _stmts;

    MYSQL* _instance;

    MysqlProxy(const MysqlProxy&);                
    MysqlProxy& operator=(const MysqlProxy&);
};

}

#endif  //__MYSQL_PROXY_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
