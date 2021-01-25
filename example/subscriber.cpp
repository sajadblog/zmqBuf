#include <iostream>

#include "../ZmqBuf.hpp"
#include "proto/msg.pb.h"
#include "functional"

ZmqBuf zmqBuf;
class Subscriber{
public :
    void testResponder(zmq::message_t *receivedData)
    {
        msgProto::TestMessage msg = zmqBuf.decodeMessage<msgProto::TestMessage>(receivedData);
        std::cout << "new subscribe message received --> "  << msg.count() << " --> "  <<  msg.msg() << std::endl;
    }
    Subscriber(){}
};

int main(int argc, char *argv[])
{
    Subscriber subscriber;
    auto startSubscribing = std::bind(&Subscriber::testResponder, subscriber, std::placeholders::_1);
    zmqBuf.subscribe("127.0.0.1:30002", "TestTopic", startSubscribing);

    while(true)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}
