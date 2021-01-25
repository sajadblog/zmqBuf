#include <iostream>

#include "../ZmqBuf.hpp"
#include "proto/msg.pb.h"
#include "functional"

class Responder{
public :
    msgProto::NewClientRep testResponder(msgProto::NewClientReq req)
    {
        std::cout << req.clientid() << " --> new request --> " << req.msg()  << std::endl;
        msgProto::NewClientRep rep;
        rep.set_clientid(req.clientid());
        rep.set_msg("World !!");
        std::cout << rep.clientid() << " --> send respond --> " << rep.msg() << std::endl;
        return rep;
    }
    Responder(){}
};

int main(int argc, char *argv[])
{
    Responder responder ;

    ZmqBuf zmqBuf ;
    auto startRespond = std::bind(&Responder::testResponder, responder, std::placeholders::_1);
    auto clientRequestFuture = zmqBuf.createResponder<msgProto::NewClientReq, msgProto::NewClientRep>(30001, startRespond, true);

    clientRequestFuture.wait();

    return 0;
}
