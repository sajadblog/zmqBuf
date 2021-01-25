#ifndef ZMQMANAGER_H
#define ZMQMANAGER_H

#include <thread>
#include <future>
#include <functional>
#include <memory>
#include <iostream>
#include <unordered_map>

#include "zmq.hpp"
#include "zmq_addon.hpp"

#include <google/protobuf/repeated_field.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/message.h>

#define gp google::protobuf

// A hash function used to hash a pair of any kind
struct hash_pair {
	template <class T1, class T2>
	size_t operator()(const std::pair<T1, T2>& p) const
	{
		auto hash1 = std::hash<T1>{}(p.first);
		auto hash2 = std::hash<T2>{}(p.second);
		return hash1 ^ hash2;
	}
};

class ZmqBuf
{
public:
    ZmqBuf(){};
    ~ZmqBuf() = default;
	template<typename REQUEST, typename REPLY>
	std::future<REPLY> createRequester(std::string address, REQUEST request);
	template<typename REQUEST, typename REPLY>
	std::future<bool> createResponder(uint32_t portNumber, std::function<REPLY (REQUEST)> responderCallback, bool unary = false);

	template<typename MESSAGE>
    void publish(std::string address, std::string topic, const MESSAGE &message);
	void subscribe(std::string address, std::string topic, std::function<void (zmq::message_t *)> subscribeCallback)
	{
        std::pair<std::string, std::string> pair(std::make_pair(address, topic));
        if(m_subscriberMap.find(pair) != m_subscriberMap.end())
			return ;
		m_subscriberMap[pair] = subscribeCallback ;
		zmq::socket_t *socket = getSubscribeSocket(address);
        socket->set(zmq::sockopt::subscribe, topic);
	}

	template<typename T>
	std::vector<T> convertEnumVector(gp::RepeatedField<int>* input);
	template<typename MESSAGE>
	zmq::message_t encodeMessage(const MESSAGE &input);
	template<typename MESSAGE>
	MESSAGE decodeMessage(const zmq::message_t *input);

private:

	std::unordered_map<std::string, zmq::socket_t*> m_publisherSocketMap;
	template <typename T> zmq::socket_t* getPublisherSocket(std::string address);

	std::unordered_map<std::pair<std::string, std::string>, std::function<void (zmq::message_t *)>,hash_pair> m_subscriberMap;
	std::unordered_map<std::string, zmq::socket_t*> m_subscribeSocketMap;
	zmq::socket_t* getSubscribeSocket(std::string address)
	{
		auto socketIt = m_subscribeSocketMap.find(address);
		if( socketIt != m_subscribeSocketMap.end())
			return socketIt->second;

		static zmq::context_t context(1);
		zmq::socket_t *socket = new zmq::socket_t(context, zmq::socket_type::sub);

		static std::future fet = std::async(std::launch::async, &ZmqBuf::inThreadCreateSubscriber, this, address);

		m_subscribeSocketMap.insert({address, socket});

		return socket;
	}


	//Req / Res
	template<typename REQUEST, typename REPLY>
	REPLY inThreadCreateRequester(std::string address, REQUEST request);
	template<typename REQUEST, typename REPLY>
	bool inThreadCreateResponder(uint32_t portNumber, std::function<REPLY (REQUEST)> responderCallback, bool unary);

	//Pub / Sub
	bool inThreadCreateSubscriber(std::string address)
	{
		zmq::socket_t *socket = getSubscribeSocket(address);
		socket->connect("tcp://" + address);
		while (1) {												//TODO : must end up at an external method call or signal
			std::vector<zmq::message_t> recv_msgs;
			zmq::recv_result_t result = zmq::recv_multipart(*socket, std::back_inserter(recv_msgs));
			assert(result && "recv failed");
			assert(*result == 2);

			zmq::message_t *msg = new zmq::message_t(std::move(recv_msgs[1]));
			m_subscriberMap[std::make_pair(address, recv_msgs.at(0).to_string())](msg);
		}
		return true;
	}
};

template <typename T>
zmq::socket_t* ZmqBuf::getPublisherSocket(std::string address)
{
	auto socketIt = m_publisherSocketMap.find(address);
	if( socketIt != m_publisherSocketMap.end())
		return socketIt->second;

	static zmq::context_t context(1);
	zmq::socket_t *socket = new zmq::socket_t(context, zmq::socket_type::pub);

	m_publisherSocketMap.insert({address, socket});

	socket->bind("tcp://" + address);

	return socket;
}

template<typename T>
std::vector<T> ZmqBuf::convertEnumVector(gp::RepeatedField<int>* input)
{
	std::vector<T> retList;
	for(int i = 0 ; i < input->size() ; i++)
		retList.push_back(static_cast<T>(input->at(i)));
	return retList;
}

template<typename MESSAGE>
MESSAGE ZmqBuf::decodeMessage(const zmq::message_t *input)// TODO : must decode from binary style not string serialization
{
	gp::io::ZeroCopyInputStream* raw_input = new gp::io::ArrayInputStream(input->data(), input->size());
	gp::io::CodedInputStream* coded_input = new gp::io::CodedInputStream(raw_input);

	uint32_t num_msg;
	coded_input->ReadLittleEndian32(&num_msg);

	std::string serialized_msg;
	uint32_t serialized_size;
	MESSAGE msg;
	coded_input->ReadVarint32(&serialized_size);
	coded_input->ReadString(&serialized_msg, serialized_size);
	msg.ParseFromString(serialized_msg);

	delete raw_input;
	delete coded_input;

	return msg;
}
template<typename MESSAGE>
zmq::message_t ZmqBuf::encodeMessage(const MESSAGE &input)// TODO : must encode to binary style not string serialization
{
	std::string encoded_message;

	gp::io::ZeroCopyOutputStream* raw_output = new gp::io::StringOutputStream(&encoded_message);
	gp::io::CodedOutputStream* coded_output = new gp::io::CodedOutputStream(raw_output);

	coded_output->WriteLittleEndian32(1);
	std::string pos_msg_serialized;
	input.SerializeToString(&pos_msg_serialized);

	coded_output->WriteVarint32(pos_msg_serialized.size());
	coded_output->WriteString(pos_msg_serialized);

	delete coded_output;
	delete raw_output;

	zmq::message_t message(encoded_message.size());
	memcpy ((void *) message.data(), encoded_message.c_str(), encoded_message.size());
	return message;
}

//-----------------------------------------------------Requester----------------------------------------------------------------------
template<typename REQUEST, typename REPLY>
std::future<REPLY> ZmqBuf::createRequester(std::string address, REQUEST request)
{
	return std::async(std::launch::async, &ZmqBuf::inThreadCreateRequester<REQUEST, REPLY>, this, address, request);
}

template<typename REQUEST, typename REPLY>
REPLY ZmqBuf::inThreadCreateRequester(std::string address, REQUEST request)
{
	zmq::context_t context(1);

	zmq::socket_t socket (context, ZMQ_REQ);
	socket.connect("tcp://" + address );

	socket.send(encodeMessage(request));

	zmq::message_t update;
	socket.recv(&update);

	REPLY reply = decodeMessage<REPLY>(&update);

	socket.close();

	return reply ;
}
//-----------------------------------------------------Responder----------------------------------------------------------------------
template<typename REQUEST, typename REPLY>
std::future<bool> ZmqBuf::createResponder(uint32_t portNumber, std::function<REPLY (REQUEST)> responderCallback, bool unary)
{
	return std::async(std::launch::async, &ZmqBuf::inThreadCreateResponder<REQUEST, REPLY>, this, portNumber, responderCallback, unary);
}

template<typename REQUEST, typename REPLY>
bool ZmqBuf::inThreadCreateResponder(uint32_t portNumber, std::function<REPLY (REQUEST)> responderCallback, bool unary)
{
	zmq::context_t context (1);
	zmq::socket_t socket (context, ZMQ_REP);
	socket.bind ("tcp://*:" + std::to_string(portNumber));

	do{
		zmq::message_t update;
		socket.recv(&update);

		REPLY reply = responderCallback(decodeMessage<REQUEST>(&update));

		socket.send(encodeMessage(reply));

	}while(!unary);										//TODO : must end up at an external method call or signal

	socket.close();
	return true;
}

//-----------------------------------------------------Publisher----------------------------------------------------------------------
template<typename MESSAGE>
void ZmqBuf::publish(std::string address, std::string topic, const MESSAGE &message)
{
	static std::mutex io_mutex;
	std::lock_guard<std::mutex> lk(io_mutex);

	zmq::socket_t *socket = getPublisherSocket<MESSAGE>(address);

	zmq::const_buffer topicBuf = zmq::const_buffer(static_cast<const char *>(topic.c_str()), (topic.length() ) * sizeof(char)) ;
	socket->send(topicBuf, zmq::send_flags::sndmore);
	socket->send(encodeMessage(message));
}

#endif // ZMQMANAGER_H
