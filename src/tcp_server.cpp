#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "tcp_server.hpp"

namespace srv {

/*----------------------------------------------------------------------
* TCPConnection Constructor
*----------------------------------------------------------------------*/
TCPConnection::TCPConnection(const int accept_sockfd)
    : created_time(timing::getTimestampSec()),
      last_beat_time(created_time),
      connection_alive(false) {
  // std::cout << "TCPConnection() accept_sockfd:" << accept_sockfd << std::endl;

  clilen = sizeof(cli_addr);
  sockfd = accept(accept_sockfd, (struct sockaddr *)&cli_addr, &clilen);

  if (sockfd == -1) {
    if (errno == EAGAIN) {
      std::cout << "ERROR EAGIN (no incoming connection)" << std::endl;
      return;
    }

    std::cout << "ERROR on accept:" << errno << std::endl;
    return;
  }

  fcntl(sockfd, F_SETFL, O_NONBLOCK);
  connection_alive = true;
}

/*----------------------------------------------------------------------
* TCPConnection Destructor
*----------------------------------------------------------------------*/
TCPConnection::~TCPConnection() {
  // std::cout << "close connection:" << sockfd << std::endl;
  if (sockfd != -1) {
    ::close(sockfd);
  }
}

/*----------------------------------------------------------------------
* TCPConnection Read
*----------------------------------------------------------------------*/
void TCPConnection::network_read() {
  int count;
  int res;

  res = ioctl(sockfd, FIONREAD, &count);
  if (res != 0) {
    std::cerr << "ERROR TCPConnection::network_read:: ioctl, errno:" << errno << std::endl;
    close();  // close connection
    return;
  }

  if (count == 0) {
    std::cerr << "ERROR TCPConnection::network_read:: count == 0" << std::endl;
    close();  // close connection
    return;
  }

  char buf[count];
  res = recv(sockfd, &buf, count, MSG_DONTWAIT);

  if (res == -1) {
    std::cerr << "ERROR TCPConnection::network_read::recv, errno:" << errno << std::endl;
    close();  // close connection
    return;
  }

  if (res == 0) {
    std::cerr << "ERROR TCPConnection::network_read::recv, res == 0:" << std::endl;
    close();  // close connection
    return;
  }

  last_beat_time = timing::getTimestampSec();
  data_in = std::string(buf, res);
}

/*----------------------------------------------------------------------
* TCPConnection Read
*----------------------------------------------------------------------*/
void TCPConnection::network_data_processed() {
  // не нужно потому что есть data_in = std::string(buf, res);
  // copy assigned constructor будет вызван
  // data_in.clear();
}

/*----------------------------------------------------------------------
* TCPConnection Write
*----------------------------------------------------------------------*/
void TCPConnection::network_write() {
  uint32_t msg_length = data_out.length();
  if (msg_length == 0) {
    return;
  }
  last_beat_time = timing::getTimestampSec();

#ifdef NET_MAX_PACKET_SIZE  // send data by chunks
  // currently i dont know which variant is better
  std::string chunk = data_out.substr(0, NET_MAX_PACKET_SIZE);
  ssize_t res = send(sockfd, chunk.c_str(), chunk.length(), 0);

  if (res == -1 || res == 0) {
    std::cout << "ERROR on send network_write()" << std::endl;
    data_out.clear();  // mark connection as nothing to send
    close();           // mark connections as dead
    return;
  }

  if (msg_length > NET_MAX_PACKET_SIZE) {
    // std::cout << "shrink data_out" << std::endl;
    // std::string new_data = data_out.substr(NET_MAX_PACKET_SIZE);
    // data_out.swap(new_data);
    data_out = data_out.substr(NET_MAX_PACKET_SIZE);
  } else {
    // we are finished with transmitting response
    // dont closing connection here cause it could be a dialog...
    //  read - write - read - write - read - write - close
    data_out.clear();
  }
#else  // ------------- without NET_MAX_PACKET_SIZE (send whole data at once)
  // send data without chunking
  ssize_t res = send(sockfd, data_out.c_str(), data_out.length(), MSG_DONTWAIT);

  if (res == -1 || res == 0) {
    std::cout << get_client_address()
              << " ERROR on send network_write(), data.length:" << data_out.length()
              << ", res:" << res << ", errno:" << errno << std::endl;
    data_out.clear();  // mark connection as nothing to send
    close();           // mark connections as dead
    return;
  }

  data_out.clear();
#endif
}

/*----------------------------------------------------------------------
* TCPConnection close
*----------------------------------------------------------------------*/
const std::string &TCPConnection::get_data() {
  return data_in;
}

/*----------------------------------------------------------------------
* TCPConnection is_alive
*----------------------------------------------------------------------*/
bool TCPConnection::is_alive() {
  if (has_something_to_send()) {
    // connections often closed with mesage to send
    return true;
  }
  return connection_alive;
}

/*----------------------------------------------------------------------
* TCPConnection reset
*----------------------------------------------------------------------*/
void TCPConnection::reset() {
  close();
  data_out.clear();
}

/*----------------------------------------------------------------------
* TCPConnection close
*----------------------------------------------------------------------*/
void TCPConnection::close() {
  // dont close right here, because it could be outgoung write data in the buffer
  connection_alive = false;
}

/*----------------------------------------------------------------------
* TCPConnection close(response)
*----------------------------------------------------------------------*/
void TCPConnection::close(const std::string msg) {
  write(msg);
  close();
}

/*----------------------------------------------------------------------
* Connection write
*----------------------------------------------------------------------*/
void TCPConnection::write(const std::string out) {
  data_out += out;
}

/*----------------------------------------------------------------------
* Connection sendbuf checker
*----------------------------------------------------------------------*/
bool TCPConnection::has_something_to_send() {
  return data_out.length() > 0;
}

/*----------------------------------------------------------------------
* Connection socket accesor
*----------------------------------------------------------------------*/
uint16_t TCPConnection::get_socket() {
  return sockfd;
}

/*----------------------------------------------------------------------
* Connection socket accesor
*----------------------------------------------------------------------*/
std::string TCPConnection::get_client_address() {
  return inet_addr_to_string(cli_addr);
}

/*----------------------------------------------------------------------
* inet_addr_to_string (mainly for printing)
*----------------------------------------------------------------------*/
std::string inet_addr_to_string(struct sockaddr_in &hostaddr) {
  char buf[INET_ADDRSTRLEN];

  const char *res = inet_ntop(AF_INET, &(hostaddr.sin_addr), (char *)buf, INET_ADDRSTRLEN);

  if (res != nullptr) {
    return std::string(buf) + ":" + std::to_string(htons(hostaddr.sin_port));
  }

  std::cout << "ERROR inet_addr_to_string() cant resolve ip address:" << errno << std::endl;
  return "-";
}
}
