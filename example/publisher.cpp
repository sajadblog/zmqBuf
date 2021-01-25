#include <iostream>

#include "../ZmqBuf.hpp"
#include "proto/msg.pb.h"
#include "functional"


int main(int argc, char *argv[])
{
    bool m_running = true;
    int i = 0 ;
    ZmqBuf zmgBuf;
    while(m_running)
    {
        msgProto::TestMessage msg;
        msg.set_count(i++);
        msg.set_msg("test message ");
        zmgBuf.publish<msgProto::TestMessage>("127.0.0.1:30002","TestTopic" , msg);

        std::cout << "new message published --> "  << msg.count() << " --> "  <<  msg.msg() << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
