#ifndef SRC_TCP_SERVER_HPP
#define SRC_TCP_SERVER_HPP

#include <cinttypes>
#include <iostream>
#include <list>
#include <vector>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <fcntl.h>
#include <poll.h>

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
#define NET_PACKET_SIZE 100000
#define ACCEPT_QUEUE_LENGTH 100
#define MAX_CONNECTION_LIFETIME_SEC 5
#define POLL_TIMEOUT_MS 5000

// class Connection;

typedef std::string NetData;
// typedef void (ConnectionProcessor)(Connection& conn);
std::string inet_addr_to_string(struct sockaddr_in& hostaddr);
/*----------------------------------------------------------------------
* TCPConnection
*----------------------------------------------------------------------*/
class TCPConnection {
 protected:
  TCPConnection(const int sockfd);

 public:
  ~TCPConnection();

  // static bool is_dead(TCPConnection* conn);

  void close();
  void close(std::string);
  void write(std::string);
  bool is_alive();
  std::string get_data();
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

/*----------------------------------------------------------------------
* TCPServer
*----------------------------------------------------------------------*/
template <typename Context>
class TCPServer {
 protected:
  class Connection : public TCPConnection {
   public:
    Connection(const int sockfd) : TCPConnection(sockfd) {
    }
    Context context;
  };

  TCPServer(uint16_t port);

 public:
  void process();
  std::string get_server_address();
  virtual void on_data(Connection& conn) = 0;
  virtual void on_connect(Connection& conn) = 0;

 private:
  void accept_new_connection();

  std::list<Connection*> connections;

  int srv_sockfd;
  struct sockaddr_in serv_addr;
};

// IMPLEMENTAION ->>>>>>>>>>>>>>>>>>>>>>>

/*----------------------------------------------------------------------
* TCPServer init
*----------------------------------------------------------------------*/
template <typename Context>
TCPServer<Context>::TCPServer(uint16_t port) {
  srv_sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (srv_sockfd == -1) {
    perror("ERROR opening socket");
    exit(-1);
  }

  /* Initialize socket structure */
  bzero(&serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  /* Now bind the host address using bind() call.*/
  if (bind(srv_sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR on binding");
    exit(-1);
  }

  int res = listen(srv_sockfd, ACCEPT_QUEUE_LENGTH);

  if (res != 0) {
    perror("ERROR on listening");
    exit(-1);
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
  // std::cout << "accept_new_connection:" << srv_sockfd << std::endl;
  Connection* conn = new Connection(srv_sockfd);

  if (!conn->is_alive()) {
    std::cout << "accept_new_connection() NOT ALIVE:" << srv_sockfd << std::endl;
    delete conn;
    return;
  }

  connections.push_back(conn);
  // call virtual metod to let parent class know about new connection
  // and init connection context
  on_connect(*conn);
}

/*----------------------------------------------------------------------
* TCPServer process tick
*----------------------------------------------------------------------*/
template <typename Context>
void TCPServer<Context>::process() {
  uint32_t current_time = timing::getTimestampSec();

  // ------------------------------------------------------
  // remove all destroyed handlers from polling cycle
  std::list<Connection*> alive_connections;

  for (typename std::list<Connection*>::iterator conn_it = connections.begin();
       conn_it != connections.end(); ++conn_it) {
    Connection* conn = *conn_it;

    if (!conn->is_alive()) {
      delete conn;
    } else {
      alive_connections.push_back(conn);
    }
  }
  // update live connections list
  connections = alive_connections;

  // ------------------------------------------------------
  // setup descriptors we will be looking for
  nfds_t nfds = connections.size() + 1;
  pollfd pfd[nfds];
  Connection* p_connections[nfds];  // pointer for fast access

  // listening for incoming connection socket
  pfd[0].fd = srv_sockfd;
  pfd[0].events = POLLIN;

  // and all reading/writing sockets
  int i = 1;

  for (typename std::list<Connection *>::iterator conn_it = connections.begin();
       conn_it != connections.end(); ++conn_it, ++i) {
    Connection* conn = *conn_it;
    p_connections[i] = conn;

    pfd[i].fd = conn->get_socket();
    pfd[i].events = POLLIN;

    if (conn->has_something_to_send()) {
      pfd[i].events |= POLLOUT;
    }

    if (current_time - conn->created_time > MAX_CONNECTION_LIFETIME_SEC) {
      std::cerr << "MAX_CONNECTION_LIFETIME_SEC:" << MAX_CONNECTION_LIFETIME_SEC << std::endl;
      conn->close();
    }
  }
  // std::cout << "connections length:" << i << std::endl;

  // ------------------------------------------------------
  // wait for incoming event
  int retval = poll(pfd, nfds, POLL_TIMEOUT_MS);
  // std::cout << "poll res:" << retval << std::endl;

  if (retval == -1) {
    std::cout << "poll() error";
    return;
  }

  if (retval == 0) {
    std::cout << "No data within (n) seconds. length:" << connections.size() << std::endl;
    return;
  }

  // ------------------------------------------------------
  // somebody need to be procesed. let's search this one
  // i == 0 => main socket listening for new connections
  for (i = 1; i < nfds; i++) {
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
  if (pfd[0].revents & POLLIN) {
    accept_new_connection();
  }
}

/*----------------------------------------------------------------------
* TCPServer get_server_address
*----------------------------------------------------------------------*/
template <typename Context>
std::string TCPServer<Context>::get_server_address() {
  return inet_addr_to_string(serv_addr);
}
}
#endif