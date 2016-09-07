#ifndef SRC_TCP_SERVER_HPP
#define SRC_TCP_SERVER_HPP

#include <cinttypes>
#include <iostream>
#include <list>

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "timing.hpp"

namespace srv {
/*
TCPServer
 Connection
  read_handler (may close() connection)
  write_handler (may close() connection)
 Connection
  read_handler
 Connection
  write_handler
 Connection
  (no handlers->close())


ConnectionsManager -> iterate connections -> process()
Connection::process() -> iterate file descriptors -> handle -> process()

io_handler could add another handlers
couldbe one one reader and one writer
Connection has activity attribute and could be close on inactivity
io_handlers updates connection activity attribute
io_handler close() do nothing, if all io_handkers closed -> connection->close()
*/
// #define NET_MAX_PACKET_SIZE 6000
#define ACCEPT_QUEUE_LENGTH 100
#define MAX_CONNECTION_LIFETIME_SEC 5
#define POLL_TIMEOUT_MS 5000

using NetData = std::string;  // net bytes is an std::string instance
std::string inet_addr_to_string(struct sockaddr_in& hostaddr);

/*----------------------------------------------------------------------
* TCPConnection
*----------------------------------------------------------------------*/
class TCPConnection {
 protected:
  TCPConnection(const int sockfd);

 public:
  ~TCPConnection();

  void close();
  void close(const std::string);
  void write(const std::string);
  bool is_alive();
  std::string& get_data();
  bool has_something_to_send();
  uint16_t get_socket();

  // -- actual net send/recv --
  void network_read();
  void network_write();
  void network_data_processed();
  std::string get_client_address();

  const uint32_t created_time;

 private:
  std::string client_addr;

  NetData data_in;
  NetData data_out;

  int sockfd;
  struct sockaddr_in cli_addr;
  socklen_t clilen;
  bool connection_alive;
};

template <typename Context>
class TCPServer;
/*----------------------------------------------------------------------
* TCPServer  virtual class for tcp connections processing
*----------------------------------------------------------------------*/
template <typename Context>
class TCPServer {
 protected:
  class Connection : public TCPConnection {
   public:
    Connection(const int sockfd) : TCPConnection(sockfd) {
    }
    // connection related context (http::HttpParser for example)
    Context context;
  };

  TCPServer(const uint16_t port);

 public:
  uint16_t process();  // return number of active connections
  std::string get_server_address();
  std::list<Connection*> get_alive_connections();

  // must be implemented in derived class
  virtual void on_data(Connection& conn) = 0;
  virtual void on_connect(Connection& conn) = 0;

 private:
  void accept_new_connection();

  std::list<Connection*> connections;

  int srv_sockfd;
  struct sockaddr_in serv_addr;
};

// class templates require to be instantate by every #include
// IMPLEMENTAION ->>>>>>>>>>>>>>>>>>>>>>>

/*----------------------------------------------------------------------
* TCPServer init
*----------------------------------------------------------------------*/
template <typename Context>
TCPServer<Context>::TCPServer(const uint16_t port) {
  srv_sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (srv_sockfd == -1) {
    std::cout << "ERROR opening socket:" << errno << std::endl;
    std::exit(-1);
  }

  /* Initialize socket structure */
  bzero(&serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  /* Now bind the host address using bind() call.*/
  while (bind(srv_sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cout << "ERROR on binding:" << errno << std::endl;
    sleep(5);
  }

  int res = listen(srv_sockfd, ACCEPT_QUEUE_LENGTH);

  if (res != 0) {
    std::cout << "ERROR on listening" << errno << std::endl;
    std::exit(-1);
  }

  std::cout << "listen on " << get_server_address() << " max_connections:" << ACCEPT_QUEUE_LENGTH
            << std::endl;
  fcntl(srv_sockfd, F_SETFL, O_NONBLOCK);
}

/*----------------------------------------------------------------------
* TCPServer new connection
*----------------------------------------------------------------------*/
template <typename Context>
void TCPServer<Context>::accept_new_connection() {
  Connection* conn = new Connection(srv_sockfd);

  if (!conn->is_alive()) {
    std::cout << "accept_new_connection() NOT ALIVE:" << srv_sockfd << std::endl;
    delete conn;
    return;
  }

  connections.push_back(conn);
  // call virtual metod to let derived class know about new connection
  // and init connection context
  on_connect(*conn);
}

/*----------------------------------------------------------------------
* TCPServer process tick
*----------------------------------------------------------------------*/
template <typename Context>
std::list<typename TCPServer<Context>::Connection*> TCPServer<Context>::get_alive_connections() {
  std::list<Connection*> alive_connections;

  for (auto& conn : connections) {
    if (!conn->is_alive()) {
      delete conn;
    } else {
      alive_connections.push_back(conn);
    }
  }

  return alive_connections;
}

/*----------------------------------------------------------------------
* TCPServer process tick
*----------------------------------------------------------------------*/
template <typename Context>
uint16_t TCPServer<Context>::process() {
  uint32_t current_time = timing::getTimestampSec();

  // ------------------------------------------------------
  // setup descriptors we will be listening for
  nfds_t nfds = connections.size() + 1;
  pollfd pfd[nfds];
  pollfd* pfd_main_socket = &pfd[0];
  Connection* p_connections[nfds];  // pointer for fast access

  // listening for incoming connection socket
  pfd_main_socket->fd = srv_sockfd;
  pfd_main_socket->events = POLLIN;

  // and all reading/writing sockets
  int i = 1;

  for (auto& conn : connections) {
    p_connections[i] = conn;

    pfd[i].fd = conn->get_socket();
    pfd[i].events = POLLIN;

    if (conn->has_something_to_send()) {
      pfd[i].events |= POLLOUT;
    }

    if (current_time - conn->created_time > MAX_CONNECTION_LIFETIME_SEC) {
      std::cerr << "ERROR MAX_CONNECTION_LIFETIME_SEC:" << MAX_CONNECTION_LIFETIME_SEC << std::endl;
      conn->close();
    }
    i++;
  }

  // ------------------------------------------------------
  // wait for incoming event
  int retval = poll(pfd, nfds, POLL_TIMEOUT_MS);

  if (retval == -1) {
    if (errno != EINTR) {  // if not a signal
      std::cout << "poll() error retval == -1, errno:" << errno << std::endl;
    }
    return connections.size();
  }

  if (retval == 0) {
    // std::cout << "No data within (n) seconds. length:" << connections.size() << std::endl;
    return connections.size();
  }

  // ------------------------------------------------------
  // somebody need to be procesed. let's search this one
  // i == 0 => main socket listening for new connections
  for (i = 1; i < nfds; ++i) {
    if (pfd[i].revents & POLLIN) {
      // fill input buffers with data
      p_connections[i]->network_read();

      // call virtual method to let parrent class process
      // inboud data with access to custom context
      on_data(*p_connections[i]);

      // clear input buffers
      p_connections[i]->network_data_processed();
    }
    if (pfd[i].revents & POLLOUT) {
      // write output buffer to network and clear them
      p_connections[i]->network_write();
    }
  }

  // ------------------------------------------------------
  // if there are new connections -> accept them
  // do it after read/write processing because
  // accept new connection will add nec connection to the std::list<Connection>
  if (pfd_main_socket->revents & POLLIN) {
    accept_new_connection();
  }

  // ------------------------------------------------------
  // remove all destroyed handlers from polling cycle
  // should be done after processing
  // update live connections list
  connections = std::move(get_alive_connections());

  return connections.size();
}

/*----------------------------------------------------------------------
* TCPServer get_server_address
*----------------------------------------------------------------------*/
template <typename Context>
std::string TCPServer<Context>::get_server_address() {
  return inet_addr_to_string(serv_addr);
}
}  // namespace srv
#endif