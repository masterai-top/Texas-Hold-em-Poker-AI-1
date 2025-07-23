#include "NetMsg.h"

std::string ProtobufToString(const google::protobuf::Message &pbMsg) {
	std::string str;
	google::protobuf::TextFormat::PrintToString(pbMsg, &str);
	std::cout << str << std::endl;
	return str;
}
