/*************************************************************************
	> File Name: main.cpp
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月31日 星期三 18时58分16秒
 ************************************************************************/

#include"config.h"
#include"webserver.h"

int main(int argc, char *argv[])
{
	std::string username = "root";
	std::string passwd = "15195382957";
	std::string dbname = "mydb";

	WebServer server;

	Config config;
	config.parse_arg(argc, argv);

	server.init(config.PORT, username, passwd, dbname, config.LOGWrite, config.OPT_LINGER, 
		config.TRIGMode, config.sql_num, config.thread_num, config.close_log, config.actor_model);

	server.log_write();

	server.sql_pool();

	server.thread_pool();

	server.trig_mode();

	server.create_listensocket_eventListen();

	server.eventLoop();

	return 0;
}
