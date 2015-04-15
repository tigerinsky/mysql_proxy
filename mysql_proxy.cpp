 /** * @file mysql_proxy.cpp * @author way
 * @date 2015/02/01 16:36:34
 * @brief 
 *  
 **/

#include "mysql_proxy.h"

#include <time.h>
#include "glog/logging.h"
#include "google/protobuf/message.h"
#include "errmsg.h"

namespace tis {

static const int MAX_VAR_INT64_BYTES = 10;
static const int MAX_VAR_INT32_BYTES = 5;

static int _bind_para(mysql_stmt_t* stmt, va_list para);
static int _bind_result(mysql_stmt_t* stmt);
static uint8_t* _write_uint32(uint32_t value, uint8_t* target);
static uint8_t* _write_int32(int32_t value, uint8_t* target);
static uint8_t* _write_uint64(uint64_t value, uint8_t* target);
static uint8_t* _write_int64(int64_t value, uint8_t* target);
static uint8_t* _write_raw(const void* data, uint32_t size, uint8_t* target);
static int _get_float(mysql_stmt_t* stmt, MYSQL_BIND* field, int number);
static int _get_double(mysql_stmt_t* stmt, MYSQL_BIND* field, int number);
static int _get_int32(mysql_stmt_t* stmt, MYSQL_BIND* field, int number);
static int _get_uint32(mysql_stmt_t* stmt, MYSQL_BIND* field, int number);
static int _get_int64(mysql_stmt_t* stmt, MYSQL_BIND* field, int number);
static int _get_uint64(mysql_stmt_t* stmt, MYSQL_BIND* field, int number);
static int _get_bool(mysql_stmt_t* stmt, MYSQL_BIND* field, int number);
static int _get_string(mysql_stmt_t* stmt, MYSQL_BIND* field, int number);

MysqlProxy::MysqlProxy() {
    _port = 0;
    _connect_timeout = DEFAULT_CONNECT_TIMEOUT;
    _read_timeout = DEFAULT_READ_TIMEOUT;
    _write_timeout = DEFAULT_WRITE_TIMEOUT;
    _ping_interval = DEFAULT_PING_INTERVAL;
    _has_connect = false;
    _instance = NULL;
}

MysqlProxy::~MysqlProxy() {

}

int MysqlProxy::library_init(void) {
    return  0 == mysql_library_init(0, NULL, NULL) ? 0 : 1;
}

void MysqlProxy::library_end(void) {
    mysql_library_end();
}

int MysqlProxy::connect(const char* host, 
                        uint32_t port,
                        const char* user,
                        const char* passwd) {
    // 1. init
    if (NULL != _instance) {
        close(); 
    }
    _instance = mysql_init(NULL);
    if (NULL == _instance) {
        return MYSQL_CONNECT_ERR; 
    }
    // 2. set para
    _host.assign(host);
    _port = port;
    _user.assign(user);
    _passwd.assign(passwd);
    if (__connect()) {
        return MYSQL_CONNECT_ERR; 
    }
    return MYSQL_CONNECT_OK;
}

int MysqlProxy::select_db(const char* db) {
    if (NULL == _instance || NULL == db) {
        return MYSQL_SELECT_DB_ERR; 
    }
    if (0 != mysql_select_db(_instance, db)){
        return MYSQL_SELECT_DB_ERR;
    }
    _db.assign(db);
    return MYSQL_SELECT_DB_OK;
}

static int _bind_para(mysql_stmt_t* stmt, va_list para) {
    stmt->arg_num = mysql_stmt_param_count(stmt->instance);
    stmt->arg = static_cast<MYSQL_BIND*>(calloc(sizeof(MYSQL_BIND), stmt->arg_num));  
    stmt->type = static_cast<int*>(malloc(sizeof(int) * stmt->arg_num));
    stmt->arg_off = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * stmt->arg_num));
    if (NULL == stmt->arg || NULL == stmt->type || NULL == stmt->arg_off) {
        return MysqlProxy::MYSQL_PREPARE_CREATE_PARA_ERR;
    }
    for (uint32_t i = 0; i < stmt->arg_num; ++i) {
        stmt->type[i] = va_arg(para, int);
        MYSQL_BIND* arg = stmt->arg + i;
        switch (stmt->type[i]) {
        case MysqlProxy::PREPARE_BOOL:
            arg->buffer_type = MYSQL_TYPE_TINY;
            break;
        case MysqlProxy::PREPARE_CHAR:
            arg->buffer_type = MYSQL_TYPE_TINY;
            break;
        case MysqlProxy::PREPARE_INT16:
            arg->buffer_type = MYSQL_TYPE_SHORT;
            break;
        case MysqlProxy::PREPARE_UINT16:
            arg->buffer_type = MYSQL_TYPE_SHORT;
            arg->is_unsigned = true;
            break;
        case MysqlProxy::PREPARE_INT32:
            arg->buffer_type = MYSQL_TYPE_LONG;
            break;
        case MysqlProxy::PREPARE_UINT32:
            arg->buffer_type = MYSQL_TYPE_LONG;
            arg->is_unsigned = true;
            break;
        case MysqlProxy::PREPARE_INT64:
            arg->buffer_type = MYSQL_TYPE_LONGLONG;
            break;
        case MysqlProxy::PREPARE_UINT64:
            arg->buffer_type = MYSQL_TYPE_LONGLONG;
            arg->is_unsigned = true;
            break;
        case MysqlProxy::PREPARE_FLOAT:
            arg->buffer_type = MYSQL_TYPE_FLOAT;
            break;
        case MysqlProxy::PREPARE_DOUBLE:
            arg->buffer_type = MYSQL_TYPE_DOUBLE;
            break;
        case MysqlProxy::PREPARE_STRING:
            arg->buffer_type = MYSQL_TYPE_STRING;
            break;
        default:
            return MysqlProxy::MYSQL_PREPARE_UNKNOWN_PARA_TYPE;
        }
    }
    return 0;
}

static int _bind_result(mysql_stmt_t* stmt) {
    if (0 == stmt->instance->field_count) {
        return 0; 
    }
    stmt->field_num = stmt->instance->field_count;
    stmt->field = static_cast<MYSQL_BIND*>(calloc(sizeof(MYSQL_BIND), stmt->field_num));  
    uint32_t buff_length = 0;
    for (uint32_t i = 0; i < stmt->field_num; ++i) {
        MYSQL_FIELD& field = stmt->instance->fields[i];
        switch (field.type) {
        case MYSQL_TYPE_TINY: 
            buff_length += 2;
            break;
        case MYSQL_TYPE_SHORT:
            buff_length += 3;
            break;
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_LONG:
            buff_length += 5;
            break;
        case MYSQL_TYPE_LONGLONG:
            buff_length += 9;
            break;
        case MYSQL_TYPE_FLOAT:
            buff_length += 5;
            break;
        case MYSQL_TYPE_DOUBLE:
            buff_length += 9;
            break;
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_BLOB:
            buff_length += (field.length + 1 + sizeof(unsigned long));
            break;
        default:
            return MysqlProxy::MYSQL_PREPARE_UNSUPPORT_FIELD_TYPE; 
        } 
    }
    stmt->bind_buff = static_cast<char*>(malloc(buff_length));
    if (NULL == stmt->bind_buff) {
        return MysqlProxy::MYSQL_PREPARE_BIND_BUFF_ERR; 
    }
    stmt->bind_buff_size = buff_length;
    
    char* p = stmt->bind_buff;
    for (uint32_t i = 0; i < stmt->field_num; ++i) {
        MYSQL_BIND& bind = stmt->field[i];
        MYSQL_FIELD& field = stmt->instance->fields[i];
        switch (field.type) {
        case MYSQL_TYPE_TINY: 
            bind.buffer_type = field.type;
            bind.is_null = p++;
            bind.buffer = p;
            p += 1;
            break;
        case MYSQL_TYPE_SHORT:
            bind.buffer_type = field.type;
            bind.is_null = p++;
            bind.buffer = p;
            p += 2;
            break;
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_LONG:
            bind.buffer_type = field.type;
            bind.is_null = p++;
            bind.buffer = p;
            p += 4;
            break;
        case MYSQL_TYPE_LONGLONG:
            bind.buffer_type = field.type;
            bind.is_null = p++;
            bind.buffer = p;
            p += 8;
            break;
        case MYSQL_TYPE_FLOAT:
            bind.buffer_type = field.type;
            bind.is_null = p++;
            bind.buffer = p;
            p += 4;
            break;
        case MYSQL_TYPE_DOUBLE:
            bind.buffer_type = field.type;
            bind.is_null = p++;
            bind.buffer = p;
            p += 8;
            break;
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_BLOB:
            bind.buffer_type = field.type;
            bind.is_null = p++;
            bind.buffer_length = field.length;
            bind.length = reinterpret_cast<unsigned long*>(p);
            p += sizeof(unsigned long);
            bind.buffer = p;
            p += field.length;
            break;
        default:
            return MysqlProxy::MYSQL_PREPARE_UNSUPPORT_FIELD_TYPE; 
        }     
    }
    if (mysql_stmt_bind_result(stmt->instance, stmt->field)) {
        return MysqlProxy::MYSQL_PREPARE_BIND_RESULT_ERR; 
    }
    return 0;
}

static int _enlarge_proto_buffer(mysql_stmt_t* stmt, uint32_t new_size) {
    uint8_t* new_buff = static_cast<uint8_t*>(malloc(new_size)); 
    if (NULL == new_buff) {
        return 1; 
    }
    memcpy(new_buff, stmt->proto_buff, stmt->proto_size);
    free(stmt->proto_buff);
    stmt->proto_buff = new_buff;
    stmt->proto_buff_size = new_size;
    return 0;
}

static int _get_float(mysql_stmt_t* stmt, MYSQL_BIND* field, int number) {
    if (*(field->is_null)) {
        return 0; 
    }
    uint32_t new_size = MAX_VAR_INT32_BYTES + stmt->proto_size + sizeof(float);
    if (stmt->proto_buff_size < new_size) {
        if (_enlarge_proto_buffer(stmt, new_size)) return 1;
    }
    uint8_t* p = stmt->proto_buff + stmt->proto_size;
    p = _write_uint32(number << 3 | 5, p);
    p = _write_raw(field->buffer, sizeof(float), p);
    stmt->proto_size = p - stmt->proto_buff; 
    return 0;
}

static int _get_double(mysql_stmt_t* stmt, MYSQL_BIND* field, int number) {
    if (*(field->is_null)) {
        return 0; 
    }
    uint32_t new_size = MAX_VAR_INT32_BYTES + stmt->proto_size + sizeof(double);
    if (stmt->proto_buff_size < new_size) {
        if (_enlarge_proto_buffer(stmt, new_size)) return 1;
    }
    uint8_t* p = stmt->proto_buff + stmt->proto_size;
    p = _write_uint32(number << 3 | 1, p);
    p = _write_raw(field->buffer, sizeof(double), p);
    stmt->proto_size = p - stmt->proto_buff; 
    return 0;
}

static int _get_string(mysql_stmt_t* stmt, MYSQL_BIND* field, int number) {
    if (*(field->is_null)) {
        return 0; 
    }
    uint32_t new_size = 2 * MAX_VAR_INT32_BYTES + stmt->proto_size + *(field->length);
    if (stmt->proto_buff_size < new_size) {
        if (_enlarge_proto_buffer(stmt, new_size)) return 1;
    }
    uint8_t* p = stmt->proto_buff + stmt->proto_size;
    p = _write_uint32(number << 3 | 2, p);
    p = _write_uint32(*(field->length), p);
    p = _write_raw(field->buffer, *(field->length), p);
    stmt->proto_size = p - stmt->proto_buff; 
    return 0;
}

static int _get_bool(mysql_stmt_t* stmt, MYSQL_BIND* field, int number) {
    if (*(field->is_null)) {
        return 0; 
    }
    uint32_t new_size = 2 * MAX_VAR_INT32_BYTES + stmt->proto_size;
    if (stmt->proto_buff_size < new_size) {
        if (_enlarge_proto_buffer(stmt, new_size)) return 1;
    }
    uint8_t* p = stmt->proto_buff + stmt->proto_size;
    bool value = *(reinterpret_cast<bool*>(field->buffer));
    p = _write_uint32(number << 3 | 0, p);
    p = _write_uint32((uint32_t)value, p);
    stmt->proto_size = p - stmt->proto_buff; 
    return 0;
}

static int _get_int32(mysql_stmt_t* stmt, MYSQL_BIND* field, int number) {
    if (*(field->is_null)) {
        return 0; 
    }
    uint32_t new_size = 2 * MAX_VAR_INT32_BYTES + stmt->proto_size;
    if (stmt->proto_buff_size < new_size) {
        if (_enlarge_proto_buffer(stmt, new_size)) return 1;
    }
    int32_t value = 0;
    switch (field->buffer_type) {
    case MYSQL_TYPE_TINY:
        value = *(reinterpret_cast<int8_t*>(field->buffer));
        break;
    case MYSQL_TYPE_SHORT:
        value = *(reinterpret_cast<int16_t*>(field->buffer));
        break;
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
        value = *(reinterpret_cast<int32_t*>(field->buffer));
        break;
    default:
        return 1;
    }
    uint8_t* p = stmt->proto_buff + stmt->proto_size;
    p = _write_uint32(number << 3 | 0, p);
    p = _write_int32(value, p);
    stmt->proto_size = p - stmt->proto_buff; 
    return 0;
}

static int _get_uint32(mysql_stmt_t* stmt, MYSQL_BIND* field, int number) {
    if (*(field->is_null)) {
        return 0; 
    }
    uint32_t new_size = 2 * MAX_VAR_INT32_BYTES + stmt->proto_size;
    if (stmt->proto_buff_size < new_size) {
        if (_enlarge_proto_buffer(stmt, new_size)) return 1;
    }
    uint32_t value = 0;
    switch (field->buffer_type) {
    case MYSQL_TYPE_TINY:
        value = *(reinterpret_cast<uint8_t*>(field->buffer));
        break;
    case MYSQL_TYPE_SHORT:
        value = *(reinterpret_cast<uint16_t*>(field->buffer));
        break;
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
        value = *(reinterpret_cast<uint32_t*>(field->buffer));
        break;
    default:
        return 1;
    }
    uint8_t* p = stmt->proto_buff + stmt->proto_size;
    p = _write_uint32(number << 3 | 0, p);
    p = _write_uint32(value, p);
    stmt->proto_size = p - stmt->proto_buff; 
    return 0;
}

static int _get_int64(mysql_stmt_t* stmt, MYSQL_BIND* field, int number) {
    if (*(field->is_null)) {
        return 0; 
    }
    uint32_t new_size = MAX_VAR_INT64_BYTES + MAX_VAR_INT32_BYTES + stmt->proto_size;
    if (stmt->proto_buff_size < new_size) {
        if (_enlarge_proto_buffer(stmt, new_size)) return 1;
    }
    int64_t value = 0;
    switch (field->buffer_type) {
    case MYSQL_TYPE_TINY:
        value = *(reinterpret_cast<int64_t*>(field->buffer));
        break;
    case MYSQL_TYPE_SHORT:
        value = *(reinterpret_cast<int64_t*>(field->buffer));
        break;
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
        value = *(reinterpret_cast<int64_t*>(field->buffer));
        break;
    case MYSQL_TYPE_LONGLONG:
        value = *(reinterpret_cast<int64_t*>(field->buffer));
        break;
    default:
        return 1;
    }
    uint8_t* p = stmt->proto_buff + stmt->proto_size;
    p = _write_uint32(number << 3 | 0, p);
    p = _write_int64(value, p);
    stmt->proto_size = p - stmt->proto_buff; 
    return 0;
}

static int _get_uint64(mysql_stmt_t* stmt, MYSQL_BIND* field, int number) {
    if (*(field->is_null)) {
        return 0; 
    }
    uint32_t new_size = MAX_VAR_INT64_BYTES + MAX_VAR_INT32_BYTES + stmt->proto_size;
    if (stmt->proto_buff_size < new_size) {
        if (_enlarge_proto_buffer(stmt, new_size)) return 1;
    }
    uint64_t value = 0;
    switch (field->buffer_type) {
    case MYSQL_TYPE_TINY:
        value = *(reinterpret_cast<uint64_t*>(field->buffer));
        break;
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_SHORT:
        value = *(reinterpret_cast<uint64_t*>(field->buffer));
        break;
    case MYSQL_TYPE_LONG:
        value = *(reinterpret_cast<uint64_t*>(field->buffer));
        break;
    case MYSQL_TYPE_LONGLONG:
        value = *(reinterpret_cast<uint64_t*>(field->buffer));
        break;
    default:
        return 1;
    }
    uint8_t* p = stmt->proto_buff + stmt->proto_size;
    p = _write_uint32(number << 3 | 0, p);
    p = _write_uint64(value, p);
    stmt->proto_size = p - stmt->proto_buff; 
    return 0;
}

static int _create_map_func(const ::google::protobuf::Descriptor* descriptor,
                     mysql_stmt_t* stmt) {
    using namespace google::protobuf;
    stmt->proto_buff = static_cast<uint8_t*>(malloc(MysqlProxy::DEFAULT_PROTO_BUFF_SIZE));
    if (NULL == stmt->proto_buff) {
        return MysqlProxy::MYSQL_PREPARE_CREATE_PROTO_BUFF_ERR; 
    }
    stmt->proto_buff_size = MysqlProxy::DEFAULT_PROTO_BUFF_SIZE;
    stmt->proto_size = 0;

    stmt->map_info = static_cast<proto_map_info_t*>(malloc(sizeof(proto_map_info_t) * stmt->field_num));
    if (NULL == stmt->map_info) {
        return MysqlProxy::MYSQL_PREPARE_CREATE_MAP_FUNC_ERR;  
    }
    for (uint32_t i = 0; i < stmt->field_num; ++i) {
        MYSQL_FIELD& field_info = stmt->instance->fields[i];
        const FieldDescriptor* fd = descriptor->FindFieldByName(std::string(field_info.name));
        if (NULL == fd) {
            stmt->map_info[i].map_func = NULL; 
            continue;
        }
        FieldDescriptor::Type type = fd->type();
        if (FieldDescriptor::TYPE_FLOAT == type 
                && (MYSQL_TYPE_FLOAT == field_info.type)) {
            stmt->map_info[i].map_func = _get_float; 
        } else if (FieldDescriptor::TYPE_DOUBLE == type
                && (MYSQL_TYPE_DOUBLE == field_info.type)) {
            stmt->map_info[i].map_func = _get_double; 
        } else if (FieldDescriptor::TYPE_BOOL == type
                && (MYSQL_TYPE_TINY == field_info.type)) {
            stmt->map_info[i].map_func = _get_bool; 
        } else if ((FieldDescriptor::TYPE_INT32 == type
                        || FieldDescriptor::TYPE_UINT32 == type)
                && (MYSQL_TYPE_TINY == field_info.type
                        || MYSQL_TYPE_SHORT == field_info.type
                        || MYSQL_TYPE_INT24 == field_info.type
                        || MYSQL_TYPE_LONG == field_info.type)) {
            stmt->map_info[i].map_func = type == FieldDescriptor::TYPE_INT32 ? _get_int32 : _get_uint32; 
        } else if ((FieldDescriptor::TYPE_INT64 == type
                        || FieldDescriptor::TYPE_UINT64 == type) 
                && (MYSQL_TYPE_TINY == field_info.type
                        || MYSQL_TYPE_SHORT == field_info.type
                        || MYSQL_TYPE_INT24 == field_info.type
                        || MYSQL_TYPE_LONG == field_info.type
                        || MYSQL_TYPE_LONGLONG == field_info.type)) {
            stmt->map_info[i].map_func = type == FieldDescriptor::TYPE_INT64 ? _get_int64 : _get_uint64; 
        } else if (FieldDescriptor::TYPE_STRING == type
                && (MYSQL_TYPE_BLOB == field_info.type
                        || MYSQL_TYPE_STRING == field_info.type
                        || MYSQL_TYPE_VAR_STRING == field_info.type)) {
            stmt->map_info[i].map_func = _get_string;
        } else {
            return MysqlProxy::MYSQL_PREPARE_ORMTYPE_NOT_MATCH;  
        }
        stmt->map_info[i].number = fd->number();
    }
    return 0;
}

int MysqlProxy::__connect() {
    my_bool value = 1;
    LOG_IF(WARNING, 0 != mysql_options(_instance, MYSQL_OPT_COMPRESS, &value)) << "mysql proxy: set opt compress error";
    LOG_IF(WARNING, 0 != mysql_options(_instance, MYSQL_OPT_CONNECT_TIMEOUT, &_connect_timeout)) << "mysql proxy: set connect timeout error";
    LOG_IF(WARNING, 0 != mysql_options(_instance, MYSQL_OPT_READ_TIMEOUT, &_read_timeout)) << "mysql proxy: set read timeout error";
    LOG_IF(WARNING, 0 != mysql_options(_instance, MYSQL_OPT_WRITE_TIMEOUT, &_write_timeout)) << "mysql proxy: set write timeout error";
    if (!mysql_real_connect(_instance, 
                            _host.c_str(), 
                            _user.c_str(), 
                            _passwd.c_str(), 
                            NULL, 
                            _port, 
                            NULL, 
                            0)){
        return 1; 
    }
    return 0;
}

int MysqlProxy::reconnect() {
    close();
    _instance = mysql_init(NULL);
    if (NULL == _instance) {
        return MYSQL_RECONN_MYSQL_ERR; 
    }
    if (__connect()) {
        return MYSQL_RECONN_MYSQL_ERR; 
    }
    if (0 != mysql_select_db(_instance, _db.c_str())){
        return MYSQL_RECONN_MYSQL_ERR;
    }
    if (_charset.length() != 0 
            && mysql_set_character_set(_instance, _charset.c_str())) {
        return MYSQL_RECONN_MYSQL_ERR; 
    }
    for (int i = 0; i < _stmts.size(); ++i) {
        mysql_stmt_t* stmt = _stmts[i]; 
        (void)mysql_stmt_close(stmt->instance); 
        stmt->instance = mysql_stmt_init(_instance);
        if (mysql_stmt_prepare(stmt->instance, stmt->query.c_str(), stmt->query.size())) {
            return MysqlProxy::MYSQL_RECONN_INIT_STMT_ERR; 
        }
        if (0 != stmt->instance->field_count) {
            if (mysql_stmt_bind_result(stmt->instance, stmt->field)) {
                return MysqlProxy::MYSQL_RECONN_INIT_STMT_ERR; 
            }
        }
    }
    return 0;
}

int MysqlProxy::prepare(const char* query, 
                        const ::google::protobuf::Descriptor* descriptor, 
                        mysql_stmt_t* stmt,
                        ...) {
    int ret = -1;
    // 1. init
    stmt->instance = mysql_stmt_init(_instance);
    if (NULL == stmt->instance) {
        return MYSQL_PREPARE_INIT_ERR;
    }
    // 2. parse
    if (mysql_stmt_prepare(stmt->instance, query, strlen(query))) {
        return MYSQL_PREPARE_PARSE_QUERY_ERR;
    }
    stmt->query = query; 
    // 3. prepare para
    va_list type_list;
    va_start(type_list, stmt); 
    ret = _bind_para(stmt, type_list);
    va_end(type_list);
    if (ret) return ret; 
    // 4. bind result
    ret = _bind_result(stmt);
    if (ret) return ret;
    // 5. orm; 
    if (descriptor) {
        ret = _create_map_func(descriptor, stmt);
        if (ret)  return ret;
    }

    _stmts.push_back(stmt);
    return MYSQL_PREPARE_OK;
}

void MysqlProxy::free_prepare(mysql_stmt_t* stmt) {
    if (NULL == stmt) return; 
    if (NULL != stmt->arg) free(stmt->arg);
    if (NULL != stmt->type) free(stmt->type); 
    if (NULL != stmt->arg_off)  free(stmt->arg_off);
    if (NULL != stmt->field) free(stmt->field); 
    if (NULL != stmt->bind_buff) free(stmt->bind_buff);
    if (NULL != stmt->map_info) free(stmt->map_info); 
    if (NULL != stmt->proto_buff) free(stmt->proto_buff); 
    if (NULL != stmt->instance) mysql_stmt_close(stmt->instance);
}

int MysqlProxy::execute(mysql_stmt_t* stmt, ...) {
    int ret = -1;
    int mysql_ret = -1;
    if (NULL == _instance || NULL == stmt) {
        return MYSQL_QUERY_ERR;
    }
    va_list value_list;
    va_start(value_list, stmt); 
    ret = __execute(stmt, value_list);
    va_end(value_list); 
    mysql_ret = mysql_errno(_instance);
    if (MYSQL_QUERY_ERR == ret 
            && (CR_SERVER_LOST == mysql_ret || CR_SERVER_GONE_ERROR == mysql_ret)) {
        if (MYSQL_RECONN_OK != reconnect()) {
            ret = MYSQL_QUERY_RECONN_ERR; 
        } else {
            va_start(value_list, stmt); 
            ret = __execute(stmt, value_list); 
            va_end(value_list); 
        }
    }
    return ret;
}

int MysqlProxy::__execute(mysql_stmt_t* stmt, va_list value_list) {
    if (free_result(stmt)) {
        return MYSQL_QUERY_FREE_RESULT_ERR;
    }
    char* p = NULL;
    uint32_t value_len = 0;
    char char_value = 0;
    int16_t int16_value = 0;
    int32_t int32_value = 0;
    int64_t int64_value = 0;
    float float_value = 0.0;
    double double_value = 0.0;
    stmt->para_buff.clear();
    for (uint32_t i = 0; i < stmt->arg_num; ++i) {
        MYSQL_BIND& arg = stmt->arg[i];
        switch (stmt->type[i]) {
        case PREPARE_BOOL:  
        case PREPARE_CHAR:
            char_value = (char)(va_arg(value_list, int));
            p = &char_value; 
            value_len = 1;
            break;
        case PREPARE_INT16:
        case PREPARE_UINT16:
            int16_value = (int16_t)va_arg(value_list, int);
            p = reinterpret_cast<char*>(&int16_value);
            value_len = 2;
            break;
        case PREPARE_INT32:
        case PREPARE_UINT32:
            int32_value = va_arg(value_list, int32_t);
            p = reinterpret_cast<char*>(&int32_value);
            value_len = 4;
            break;
        case PREPARE_FLOAT:
            float_value = (float)va_arg(value_list, double);
            p = reinterpret_cast<char*>(&float_value);
            value_len = 4;
            break;
        case PREPARE_INT64:
        case PREPARE_UINT64:
            int64_value = va_arg(value_list, int64_t);
            p = reinterpret_cast<char*>(&int64_value);
            value_len = 8;
            break;
        case PREPARE_DOUBLE:
            double_value = va_arg(value_list, double);
            p = reinterpret_cast<char*>(&double_value);
            value_len = 8;
            break;
        case PREPARE_STRING:
            p = va_arg(value_list, char*);
            value_len = strlen(p);
            arg.buffer_length = value_len;
        default:
            break; 
        }
        stmt->arg_off[i] = stmt->para_buff.size(); 
        stmt->para_buff.append(p, value_len);
    }

    for (uint32_t i = 0; i < stmt->arg_num; ++i) {
        MYSQL_BIND& arg = stmt->arg[i];
        arg.buffer = const_cast<char*>(stmt->para_buff.c_str() + stmt->arg_off[i]);
    }

    if (mysql_stmt_bind_param(stmt->instance, stmt->arg)) {
        return MYSQL_PREPARE_BIND_PARA_ERR;
    }

    if (mysql_stmt_execute(stmt->instance)) {
        return MYSQL_QUERY_ERR; 
    } 
    if (0 < mysql_stmt_field_count(stmt->instance)) {
        if (mysql_stmt_store_result(stmt->instance) ) {
            return MYSQL_QUERY_ERR; 
        }
    }

    return MYSQL_QUERY_OK;
}

int MysqlProxy::free_result(mysql_stmt_t* stmt) {
    if (NULL != stmt && NULL != stmt->instance) {
        return mysql_stmt_free_result(stmt->instance);
    }
    return 0;
}

void MysqlProxy::close(void) {
    if (NULL != _instance) {
        mysql_close(_instance);
        _instance = NULL;
    }
}

uint64_t MysqlProxy::get_row_num(mysql_stmt_t* stmt) {
    return mysql_stmt_num_rows(stmt->instance);
}

uint32_t MysqlProxy::get_field_num(mysql_stmt_t* stmt) {
    return mysql_stmt_field_count(stmt->instance);  
}

const char* MysqlProxy::get_err_msg(void) {
    return _instance ? mysql_error(_instance) : "";
}

const char* MysqlProxy::get_prepare_err_msg(const mysql_stmt_t* stmt) {
    return stmt ? mysql_stmt_error(stmt->instance) : "";
}

int MysqlProxy::next(mysql_stmt_t* stmt) {
    int ret = mysql_stmt_fetch(stmt->instance);
    switch (ret) {
    case 0: 
        return MYSQL_NEXT_OK;
    case MYSQL_NO_DATA:
        return MYSQL_NEXT_END;
    default:
        return MYSQL_NEXT_ERR;
    }
}

uint64_t MysqlProxy::get_affected_row_num (mysql_stmt_t* stmt) {
    return mysql_stmt_affected_rows(stmt->instance); 
}

uint64_t MysqlProxy::get_insert_id(mysql_stmt_t* stmt) {
    return mysql_stmt_insert_id(stmt->instance); 
}

static uint8_t* _write_uint32(uint32_t value, uint8_t* target) {
    target[0] = static_cast<uint8_t>(value | 0x80);
    if (value >= (1 << 7)) {
        target[1] = static_cast<uint8_t>((value >>  7) | 0x80);
        if (value >= (1 << 14)) {
            target[2] = static_cast<uint8_t>((value >> 14) | 0x80);
            if (value >= (1 << 21)) {
                target[3] = static_cast<uint8_t>((value >> 21) | 0x80);
                if (value >= (1 << 28)) {
                    target[4] = static_cast<uint8_t>(value >> 28);
                    return target + 5;
                } else {
                    target[3] &= 0x7F;
                    return target + 4;
                }
            } else {
                target[2] &= 0x7F;
                return target + 3;
            }
        } else {
            target[1] &= 0x7F;
            return target + 2;
        }
    } else {
        target[0] &= 0x7F;
        return target + 1;
    }
}

static uint8_t* _write_uint64(uint64_t value, uint8_t* target) {
    uint32_t part0 = static_cast<uint32_t>(value      );
    uint32_t part1 = static_cast<uint32_t>(value >> 28);
    uint32_t part2 = static_cast<uint32_t>(value >> 56);

    int size;

    if (part2 == 0) {
        if (part1 == 0) {
            if (part0 < (1 << 14)) {
                if (part0 < (1 << 7)) {
                    size = 1; goto size1;
                } else {
                    size = 2; goto size2;
                }
            } else {
                if (part0 < (1 << 21)) {
                    size = 3; goto size3;
                } else {
                    size = 4; goto size4;
                }
            }
        } else {
            if (part1 < (1 << 14)) {
                if (part1 < (1 << 7)) {
                    size = 5; goto size5;
                } else {
                    size = 6; goto size6;
                }
            } else {
                if (part1 < (1 << 21)) {
                    size = 7; goto size7;
                } else {
                    size = 8; goto size8;
                }
            }
        }
    } else {
        if (part2 < (1 << 7)) {
            size = 9; goto size9;
        } else {
            size = 10; goto size10;
        }
    }

    LOG(FATAL) << "Can't get here.";

    size10: target[9] = static_cast<uint8_t>((part2 >>  7) | 0x80);
    size9 : target[8] = static_cast<uint8_t>((part2      ) | 0x80);
    size8 : target[7] = static_cast<uint8_t>((part1 >> 21) | 0x80);
    size7 : target[6] = static_cast<uint8_t>((part1 >> 14) | 0x80);
    size6 : target[5] = static_cast<uint8_t>((part1 >>  7) | 0x80);
    size5 : target[4] = static_cast<uint8_t>((part1      ) | 0x80);
    size4 : target[3] = static_cast<uint8_t>((part0 >> 21) | 0x80);
    size3 : target[2] = static_cast<uint8_t>((part0 >> 14) | 0x80);
    size2 : target[1] = static_cast<uint8_t>((part0 >>  7) | 0x80);
    size1 : target[0] = static_cast<uint8_t>((part0      ) | 0x80);

    target[size-1] &= 0x7F;
    return target + size;
}

static uint8_t* _write_int32(int32_t value, uint8_t* target) {
    return _write_uint32(static_cast<uint32_t>(value), target);
}

static uint8_t* _write_int64(int64_t value, uint8_t* target) {
    return _write_uint64(static_cast<uint64_t>(value), target);
}

static uint8_t* _write_raw(const void* data, uint32_t size, uint8_t* target) {
    memcpy(target, data, size);
    return target + size;
}

int MysqlProxy::get_proto(mysql_stmt_t* stmt, ::google::protobuf::Message* m) {
    using namespace ::google::protobuf;
    if (NULL == m || NULL == stmt) {
        return MYSQL_GET_PROTO_ERR; 
    }
    stmt->proto_size = 0;
    for (uint32_t i = 0; i < stmt->field_num; ++i) {
        MYSQL_BIND* field = stmt->field + i;
        proto_map_info_t* map_info = stmt->map_info + i;
        if (NULL == map_info->map_func) {
            continue;
        }
        if((map_info->map_func)(stmt, field, map_info->number)) {
            return MYSQL_GET_PROTO_ERR;
        }
    }
    if (!m->ParseFromArray(stmt->proto_buff, stmt->proto_size)) {

        return MYSQL_GET_PROTO_PARSE_ERR; 
    }
    return MYSQL_GET_PROTO_OK;
}

int MysqlProxy::set_charset(const char* name) {
    if (_instance && name && !mysql_set_character_set(_instance, name)) {
        _charset.assign(name);
        return MYSQL_SET_CHARSET_OK;
    } else {
        return MYSQL_SET_CHARSET_ERR; 
    }
}

}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
