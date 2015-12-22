#include <set>
#include <map>
#include <string>
#include <vector>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/logger/syslog.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using std::string;


class broadcast_server {
	typedef std::set<connection_hdl, std::owner_less<connection_hdl>> con_list;
public:
	bool getNext(string& out, string& in) {
		auto f1 = in.find("\"");
		if (f1 == string::npos) return false;
		auto f2 = in.find("\"", f1 + 1);

		out = in.substr(f1 + 1, f2 - f1 - 1);
		in = in.substr(f2 + 1);
		return true;
	}

	broadcast_server() {
		m_server.init_asio();

		m_server.set_open_handler(bind(&broadcast_server::on_open, this, ::_1));
		m_server.set_close_handler(bind(&broadcast_server::on_close, this, ::_1));
		m_server.set_message_handler(bind(&broadcast_server::on_message, this, ::_1, ::_2));
		m_server.set_fail_handler(bind(&broadcast_server::on_fail, this, ::_1));
	}

	void on_fail(connection_hdl hdl) {
		server::connection_ptr con = m_server.get_con_from_hdl(hdl);
		std::cout << "Fail handler: " << con->get_ec() << " " << con->get_ec().message() << std::endl;
	}

	void on_open(connection_hdl hdl) {
		//do nothing
	}

	void on_close(connection_hdl hdl) {
		wildcard.erase(hdl);
		for (auto it : ws)
			it.second.erase(hdl);
	}

	void on_message(connection_hdl hdl, server::message_ptr msg) {
		string str = msg->get_payload();
		string symbol;
		getNext(symbol, str);

		if (symbol == "SUBSCRIBE") {
			getNext(symbol, str);
			if (symbol == "*")
				wildcard.insert(hdl);
			else {
				ws[symbol].insert(hdl);
				while (getNext(symbol, str))
					ws[symbol].insert(hdl);
			}
		}
		else {
			send_message(wildcard, msg);
			send_message(ws[symbol], msg);
		}
	}

	void send_message(const con_list& wslist, const server::message_ptr& msg) {
		std::vector<connection_hdl> failed_hdl;
		for (auto it : wslist) {
			try {
				m_server.send(it, msg);
			}
			catch (...) {
				failed_hdl.push_back(it);
			}
		}
		
		for (auto it : failed_hdl) on_close(it);
	}

	void run(uint16_t port) {
		m_server.listen(port);
		m_server.start_accept();
		m_server.clear_access_channels(websocketpp::log::alevel::all);	// turn off logging
		m_server.run();
	}

private:
	server m_server;
	con_list wildcard;
	std::map<string, con_list> ws;
};


int main() {
	broadcast_server server;
	server.run(9002);
}