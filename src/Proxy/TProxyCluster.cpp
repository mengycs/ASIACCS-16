// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "TProxyService.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/server/TThreadedServer.h>

#include <vector>

#include "../Caravel/RedisHelper.h"
#include "../Caravel/PRF.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace caravel;
using boost::shared_ptr;
using namespace std;
using namespace  ::proxyserver;

#define SHA256_DIGEST_LENGTH 32
#define INDEX_STOP_FLAG 1234567890
#define DEBUG_TPROXY_FLAGAAAA

class TProxyServiceHandler : virtual public TProxyServiceIf {
 public:
  TProxyServiceHandler() {
    // Your initialization goes here

	  redisHelper.OpenClusterPool();

  }

  RedisHelper redisHelper;

  /**
   * ProxyGet API
   * @return Value as Binary
   * 
   * @param Trapdoor
   */
  void ProxyGet(std::string& _return, const std::string& Trapdoor) {

#ifdef DEBUG_TPROXY_FLAG
    printf("ProxyGet\n");
#endif

	uint32_t uiSize = redisHelper.ClusterPoolGet(Trapdoor, _return);

	if(0 == uiSize)
	{
		_return = "";
	}

  }

  /**
   * ProxyPut API
   * 
   * @param Trapdoor
   * @param Val
   * @param IndexTrapdoor
   * @param IndexVal
   */
  void ProxyPut(const std::string& Trapdoor, const std::string& Val, const std::string& IndexTrapdoor, const std::string& IndexVal) {

#ifdef DEBUG_TPROXY_FLAG
    printf("ProxyPut\n");
#endif

	redisHelper.ClusterPoolPut(Trapdoor, Val);

	if (0 != IndexTrapdoor.length())
	{
		redisHelper.ClusterPoolPut(IndexTrapdoor, IndexVal);

	}

  }

  /**
   * ProxyGetColumn
   * @return a binary list of Column value
   * 
   * @param IndexTrapdoor
   * @param IndexMask
   * @param GetNum
   */
  void ProxyGetColumn(std::vector<std::string> & _return, const std::string& IndexTrapdoor, const std::string& IndexMask, const int32_t GetNum) {
    
#ifdef DEBUG_TPROXY_FLAG
    printf("ProxyGetColumn\n");
#endif

	//foreach counter = 0 to GetNum
	const uint32_t kIndexValSize = SHA256_DIGEST_LENGTH + sizeof(uint32_t);
	char szTmp[kIndexValSize];
	string strIndexTrapdoor;
	string strIndexMask;
	string strTrapdoor;
	string strVal;
	for (uint32_t uiCounter = 0; uiCounter < GetNum; uiCounter++)
	{
		//Generate the Index Trapdoor (including counter)
		PRF::Sha256((char*)&uiCounter, sizeof(uiCounter), (char*)IndexTrapdoor.c_str(), IndexTrapdoor.length(), szTmp, SHA256_DIGEST_LENGTH);
		strIndexTrapdoor.assign(szTmp, SHA256_DIGEST_LENGTH);

		//Generate the Index Mask (including counter)
		PRF::Sha256((char*)&uiCounter, sizeof(uiCounter), (char*)IndexMask.c_str(), IndexMask.length(), szTmp, SHA256_DIGEST_LENGTH);
		strIndexMask.assign(szTmp, SHA256_DIGEST_LENGTH);

		string strIndexVal;
		redisHelper.ClusterPoolGet(strIndexTrapdoor, strIndexVal);
		if (strIndexVal.length() == 0)
		{
			continue;
		}

		if (INDEX_STOP_FLAG == *(uint32_t*)(strIndexVal.c_str()) ^ *(uint32_t*)(strIndexMask.c_str()))
		{
			memcpy(szTmp, strIndexMask.c_str(), SHA256_DIGEST_LENGTH);
			//set the padding to 0
			*(uint32_t*)(szTmp + SHA256_DIGEST_LENGTH) = 0;

			//strIndexVal = [{Mask}{0000}] ^ [{FLAG}{Trapdoor}]
			//foreach byte do XOR and get Trapdoor
			char *pTrapdoor = szTmp + sizeof(uint32_t);
			const char *pIndexVal = strIndexVal.c_str() + sizeof(uint32_t);
			for (uint32_t uiCur = 0; uiCur < SHA256_DIGEST_LENGTH; uiCur++)
			{
				pTrapdoor[uiCur] ^= pIndexVal[uiCur];
			}

			strTrapdoor.assign(pTrapdoor, SHA256_DIGEST_LENGTH);
			
			//Get the Value by Trapdoor
			redisHelper.ClusterPoolGet(strTrapdoor, strVal);

			if (strVal.length() == 0)
			{
				continue;
			}

			//Add to return vector
			_return.push_back(strVal);
		}
		else
		{
			return;
		}

	}

	//Close the Redis Connection

  }

};

int main(int argc, char **argv) {
  int port = 9090;
  boost::shared_ptr<TProxyServiceHandler> handler(new TProxyServiceHandler());
  boost::shared_ptr<TProcessor> processor(new TProxyServiceProcessor(handler));
  boost::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
  boost::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
  boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

  //TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
  TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);

  server.serve();
  return 0;
}

 