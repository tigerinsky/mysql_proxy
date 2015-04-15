#---------- env ----------
CXX=g++
CXXFLAGS=-D_GNU_SOURCE -D__STDC_LIMIT_MACROS -g -pipe -W -Wall -fPIC -fno-omit-frame-pointer
INCPATH=-I. -I/home/meihua/dy/src/mysql_proxy/../glog/include -I/home/meihua/dy/src/mysql_proxy/../protobuf/include -I/home/meihua/dy/src/mysql_proxy/../gflags/include -I/home/meihua/dy/src/mysql_proxy/../mysql-connector/include
LIBPATH=-Xlinker "-(" -ldl -lpthread -lm -lrt /home/meihua/dy/src/mysql_proxy/../glog/lib/libglog.a /home/meihua/dy/src/mysql_proxy/../protobuf/lib/libprotoc.a /home/meihua/dy/src/mysql_proxy/../protobuf/lib/libprotobuf.a /home/meihua/dy/src/mysql_proxy/../protobuf/lib/libprotobuf-lite.a /home/meihua/dy/src/mysql_proxy/../gflags/lib/libgflags.a /home/meihua/dy/src/mysql_proxy/../gflags/lib/libgflags_nothreads.a /home/meihua/dy/src/mysql_proxy/../mysql-connector/lib/libmysqlclient.a -Xlinker "-)"


#---------- phony ----------
.PHONY:all
all:prepare \
libmysql_proxy.a \


.PHONY:prepare
prepare:
	mkdir -p ./output/lib ./output/include

.PHONY:clean
clean:
	rm -rf /home/meihua/dy/src/mysql_proxy/mysql_proxy.o ./output


#---------- link ----------
libmysql_proxy.a:/home/meihua/dy/src/mysql_proxy/mysql_proxy.o \

	ar crs ./output/lib/libmysql_proxy.a /home/meihua/dy/src/mysql_proxy/mysql_proxy.o
	cp /home/meihua/dy/src/mysql_proxy/mysql_proxy.h ./output/include/


#---------- obj ----------
/home/meihua/dy/src/mysql_proxy/mysql_proxy.o: /home/meihua/dy/src/mysql_proxy/mysql_proxy.cpp \
 /home/meihua/dy/src/mysql_proxy/mysql_proxy.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/mysql.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/mysql_version.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/mysql_com.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/mysql_time.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/my_list.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/mysql/client_plugin.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/mysql/plugin_auth_common.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/typelib.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/my_alloc.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/mysql/psi/psi_memory.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/mysql/psi/psi_base.h \
 /home/meihua/dy/src/mysql_proxy/../glog/include/glog/logging.h \
 /home/meihua/dy/src/mysql_proxy/../gflags/include/gflags/gflags.h \
 /home/meihua/dy/src/mysql_proxy/../gflags/include/gflags/gflags_declare.h \
 /home/meihua/dy/src/mysql_proxy/../glog/include/glog/log_severity.h \
 /home/meihua/dy/src/mysql_proxy/../glog/include/glog/vlog_is_on.h \
 /home/meihua/dy/src/mysql_proxy/../protobuf/include/google/protobuf/message.h \
 /home/meihua/dy/src/mysql_proxy/../protobuf/include/google/protobuf/message_lite.h \
 /home/meihua/dy/src/mysql_proxy/../protobuf/include/google/protobuf/stubs/common.h \
 /home/meihua/dy/src/mysql_proxy/../protobuf/include/google/protobuf/descriptor.h \
 /home/meihua/dy/src/mysql_proxy/../mysql-connector/include/errmsg.h
	$(CXX) $(INCPATH) $(CXXFLAGS) -c -o /home/meihua/dy/src/mysql_proxy/mysql_proxy.o /home/meihua/dy/src/mysql_proxy/mysql_proxy.cpp


