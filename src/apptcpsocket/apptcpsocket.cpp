
#include "apptcpsocket.h"
using namespace std;
const char* file_source_root = "/home/codez/LinuxTest/TinyWebServer2.0/resources/tpdata.json";
typedef union {
    float f;
    int i;  
}ufi;
float ntohf(float f){
    ufi fx;
    fx.f = f;
    fx.i = ntohl(fx.i);
    return fx.f; 
}

APPTCP::APPTCP(){
    last_loop_length = 0;

}

APPTCP::~APPTCP(){

}

void APPTCP::socketinit(int port){

    m_tcp_port = port;
    ser_tcp_lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (ser_tcp_lfd == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        perror("socket");
        exit(0);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(m_tcp_port);

    int opt = 1;
    setsockopt(ser_tcp_lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //bind
    int ret = bind(ser_tcp_lfd, (struct sockaddr*) &serverAddress, sizeof(serverAddress));

    if(ret == -1)
    {
        std::cerr << "Failed to bind socket." << std::endl;
        close(ser_tcp_lfd);
        perror("bind");
        exit(0);
    }

    //listen
    ret = listen(ser_tcp_lfd, 10) == -1;
    if (ret == -1) {
        std::cerr << "Failed to listen on socket." << std::endl;
        close(ser_tcp_lfd);
        return;
    }

    std::cout << "Data receiver thread is running and listening on port." << m_tcp_port<<std::endl;


}

void APPTCP::dataReceiverThread() {


    while (true) {
        struct sockaddr_in cliAddr;
        socklen_t cliAddrLen = sizeof(cliAddr);
        int cli_tcp_fd = accept(ser_tcp_lfd, (struct sockaddr*) &cliAddr, &cliAddrLen);
        if (cli_tcp_fd == -1) {
            std::cerr << "Failed to accept client connection." << std::endl;
            perror("accept");
            exit(0);
        }

        std::cout << "Client connected. Client IP: " << inet_ntoa(cliAddr.sin_addr)
                  << " Client Port: " << ntohs(cliAddr.sin_port) << std::endl;

        memset(recv_buffer, 0, sizeof(recv_buffer));
        memset(last_loop_buffer,'\0',sizeof(last_loop_buffer));
        std::string receivedData;

        while (true) {
            int bytesRead = recv(cli_tcp_fd, recv_buffer, RECV_BUFFER_SZIE - 1, 0);
            if (bytesRead == -1) {
                std::cerr << "Failed to receive data from client." << std::endl;
                close(cli_tcp_fd);
                break;
            } else if (bytesRead == 0) {
                std::cout << "Client disconnected." << std::endl;
                close(cli_tcp_fd);
                break;
            }
            else if(bytesRead > 0){

                recv_buffer[bytesRead] = '\0';
                cout << " 本轮数据量： "<<bytesRead << endl;

                memcpy(splic_pack_buffer + splic_index, recv_buffer, bytesRead);
                splic_index += bytesRead;
                if(splic_index > 5139){
                    cout << "开始解析数据" << splic_index  << endl;
                    parsePkg(splic_pack_buffer, splic_index);
                    splic_index = 0;
                }
             
                
                // receivedData += recv_buffer;
                // std::cout << receivedData << std::endl;

            }

            // // 将接收到的数据放入缓存队列
            // std::lock_guard<std::mutex> lock(dataMutex);
            // dataQueue.push(receivedData);
            // receivedData.clear();

            // // 唤醒等待的线程
            // dataCondVar.notify_one();
        }
    }

    close(ser_tcp_lfd);
}

void APPTCP::parsePkg(char * buffer_tmpe, int len ){

     pack_header_t * pack_header;

    // pack_header = (pack_header_t *)(buffer_tmpe);
    // uint16_t head_check =  ntohs(pack_header -> header);
    // cout << head_check  <<endl;

    char* buffer = last_loop_buffer;
	int last_length = last_loop_length;



	if (last_length + len > (int)sizeof(last_loop_buffer))//参数越界
	{
		last_length = last_length + len - sizeof(last_loop_buffer);
	}
	if (last_length <= 0)
	{
        last_length = 0;
		buffer = buffer_tmpe;
	}
	else
	{
		memcpy(buffer + last_length, buffer_tmpe, len);
	}
    int length = last_length + len;
    int curpkg_index = 0;

    cout << "开始解大包" << endl;
    cout << length << endl;
    while ((curpkg_index < length) && (length >= 5000)){

        if (curpkg_index + (int)sizeof(pack_header_t) > length)
		{
			printf("pkg header length error parse_index,%d,\r\n", curpkg_index);
			// 不足最小包头长度
			break;
		}
        //找帧头 AA33
        pack_header = (pack_header_t *)(buffer + curpkg_index);
        cout << ntohs(pack_header -> header)<<endl;

        if ((uint16_t)DATA_HEAD != ntohs(pack_header -> header))
		{
			printf("pkg header curpkg_index, %d,\r\n", curpkg_index);
            cout << ntohs(pack_header -> header)<<endl;
			curpkg_index++;
			continue;
		}

        //确定数据长度 offset
        int data_len = ntohs(pack_header->length) ;
        int offset = curpkg_index + sizeof(pack_header_t);

        //判断是否剩余足够一包
        if (offset + data_len  > length)
		{
			//数据不足
            cout << " offset " << offset << "  datalen  " << data_len << " length " << length <<endl;
			printf("Insufficient pack data length !!\n");
			break;
		}

        //帧尾校验判断
        int offset_check = offset + data_len -1;//校验位的数据位置

		uint8_t check = *(uint8_t*)(buffer + offset_check);

		uint8_t result_check = DATA_CHECK_VALUE;

        if (check != result_check)
		{
			//校验失败
			printf("2.check fail!!\n");
		}

        else
		{
            cout << "传入解析一帧函数" << endl;
            parse_data_frame(buffer + offset, data_len);

        }
        curpkg_index = offset_check + sizeof(uint8_t);//校验位
    }

    last_loop_length = ( length - curpkg_index <= 0 ) ? 0 : (length - curpkg_index);
    if(last_loop_length > (uint8_t)sizeof(last_loop_buffer) ){
        last_loop_length = sizeof(last_loop_buffer);
    }
    if(last_loop_length > 0){
        memcpy(last_loop_buffer, buffer + curpkg_index, last_loop_length);
        cout << "  last_loop_length:   " <<  last_loop_length << endl;
    }
    
 }

void APPTCP::parse_data_frame(char * buffer_data, int length){

    char * cur_pkg_index;
    memcpy(spectrumArr, & buffer_data, SPEC_ARR_NUM * 2);
    cur_pkg_index = buffer_data + SPEC_ARR_NUM * 2;
    memcpy(recv_temperature, cur_pkg_index, 4);
    cur_pkg_index += 4;
    memcpy(recv_pressure, cur_pkg_index, 4);
    cur_pkg_index += 4;
    memcpy(timestr, cur_pkg_index, 24);
    
    string curtime = timestr; 
    // for(int i = 0; i < (int16_t)sizeof(spectrumArr); i++){
    //     spectrumArr[i] = ntohs(spectrumArr[i]);
    //     cout << spectrumArr[i] ;
    // }
    //     cout  << " " <<endl;
    Json::Value TemPre;
    TemPre["temperature"] = ntohf( *recv_temperature );
    TemPre["pressure"] =  ntohf( * recv_pressure );
    TemPre["curframetime"] = curtime;\
    string strValue = TemPre.toStyledString();      // json对象转变为json字符串
    //写文件
   // writeFileJson(TemPre);

   // cout << strValue << endl;
    // cout <<  "recv_temperature " <<ntohf( *recv_temperature )<< "  " << " recv_pressure : "<< ntohf( * recv_pressure )<< endl;
    // for(int i = 0; i < sizeof(timestr);i++){
    //     cout << timestr[i] ;
    // }
    // cout << " "<< endl;
    
    // 写入共享缓存
    std::lock_guard<std::mutex> lock(bufferMutex);
    memcpy(spec_share_buf, & spectrumArr, SPEC_ARR_NUM * 2);
    Json_share_buf = strValue;
    cout << Json_share_buf << endl;
  
    return ;
}
void APPTCP::writeFileJson( Json::Value Jsontemp){
    
	Json::StyledWriter sw;
    sw.write(Jsontemp);
    ofstream osout;
	//os.open(file_source_root, std::ios::out | std::ios::app);
    osout.open(file_source_root, std::ios::out);
	if (!osout.is_open())
		cout << "error:can not find or create the file which named \" tpdata.json\"." << endl;
	osout << sw.write(Jsontemp);
	osout.close();
}

// void dataSenderThread() {
//   

//         while (true) {
//             // 从缓存队列中获取数据
//             std::unique_lock<std::mutex> lock(dataMutex);
//             dataCondVar.wait(lock, [] { return !dataQueue.empty(); });
//             std::string dataToSend = dataQueue.front();
//             dataQueue.pop();
//             lock.unlock();

//             if (dataToSend.empty()) {
//                 // 缓存队列中没有数据，继续等待
//                 continue;
//             }

// }

// int main() {
//     APPTCP apptcpspec;
//     apptcpspec.socketinit(10010);
//     apptcpspec.dataReceiverThread();
//     return 0;
// }
