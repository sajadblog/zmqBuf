#include <iostream>

#include "../ZmqBuf.hpp"
#include "proto/msg.pb.h"
#include "functional"

int main(int argc, char *argv[])
{
    msgProto::NewClientReq req;
    req.set_clientid(0);
    req.set_msg("Hello");
    ZmqBuf zmqBuf;
    std::cout << req.clientid() << " --> send request --> " << req.msg() << std::endl;
    msgProto::NewClientRep rep = zmqBuf.createRequester<msgProto::NewClientReq, msgProto::NewClientRep>("127.0.0.1:30001", req).get();

    std::cout << rep.clientid() << " --> new respond --> " << rep.msg() << std::endl;

    return 0;
}
