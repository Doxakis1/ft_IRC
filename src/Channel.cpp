#include "Channel.hpp"
#include "Server.hpp"

Channel::Channel(std::string const &name, Client *client, Server &server) : name(name), server(server), topic_str(""), modes(0), limit(0)
{
	add_client(client);
	add_op(client);
}

void Channel::join(Client *client, std::string const &key)
{
	if (!invite_check(client))
	{
		std::cerr << "Client could not join channel: invite only" << std::endl;
		server.send_response(ERR_INVITEONLYCHAN(server.get_name(), client->get_nickname(), this->name), client->get_fd());
		return;
	}
	if (!key_check(key))
	{
		std::cerr << "Client could not join channel: wrong key" << std::endl;
		server.send_response(ERR_BADCHANNELKEY(this->name), client->get_fd());
		return;
	}
	if (!limit_check())
	{
		std::cerr << "Client could not join channel: channel is full" << std::endl;
		server.send_response(ERR_CHANNELISFULL(this->name), client->get_fd());
		return;
	}
	if (get_client(client->get_nickname()) != NULL)
	{
		std::cerr << "Client could not join channel: client already in channel" << std::endl;
		server.send_response(ERR_USERONCHANNEL(server.get_name(), client->get_nickname(), this->name), client->get_fd());
		return;
	}
	add_client(client);
	client->add_channel(this);
	broadcast(CLIENT(client->get_nickname(), client->get_username(), client->get_IPaddr()) + " JOIN " + this->name + CRLF);
	this->topic(client);
}

void Channel::invite(Client *commander, std::string const &nickname)
{
	if (!get_op(commander))
	{
		std::cerr << "Client could not invite: not an op" << std::endl;
		server.send_response(ERR_CHANOPRIVSNEEDED(this->name), commander->get_fd());
		return;
	}
	Client *client = server.get_client(nickname);
	if (client == NULL)
	{
		std::cerr << "Client could not invite: client does not exist" << std::endl;
		server.send_response(ERR_NOSUCHNICK(nickname), commander->get_fd());
		return;
	}
	if (get_invite(nickname) != NULL)
	{
		std::cerr << "Client could not invite: client already invited" << std::endl;
		server.send_response(ERR_USERONCHANNEL(server.get_name(), nickname, this->name), commander->get_fd());
		return;
	}
	add_invite(client);
	server.send_response(RPL_INVITING(commander->get_nickname(), client->get_nickname(), this->name), commander->get_fd());
	server.send_response(RPL_INVITED(CLIENT(commander->get_nickname(), commander->get_username(), commander->get_IPaddr()), client->get_nickname(), this->name), client->get_fd());
}

void Channel::kick(Client *commander, std::string const &nickname)
{
	if (!get_op(commander))
	{
		server.send_response(ERR_CHANOPRIVSNEEDED(this->name), commander->get_fd());
		return;
	}
	if (get_client(nickname) == NULL)
	{
		server.send_response(ERR_NOSUCHNICK(nickname), commander->get_fd());
		return;
	}
	remove_client(nickname);
	broadcast(RPL_KICK(CLIENT(commander->get_nickname(), commander->get_username(), commander->get_IPaddr()), this->name, nickname, ""));
	server.send_response(RPL_KICK(CLIENT(commander->get_nickname(), commander->get_username(), commander->get_IPaddr()), this->name, nickname, ""), server.get_client(nickname)->get_fd());
}

void Channel::kick(Client *commander, std::string const &nickname, std::string const &msg)
{
	if (!get_op(commander))
	{
		server.send_response(ERR_CHANOPRIVSNEEDED(this->name), commander->get_fd());
		return;
	}
	if (get_client(nickname) == NULL)
	{
		server.send_response(ERR_NOSUCHNICK(nickname), commander->get_fd());
		return;
	}
	remove_client(nickname);
	broadcast(RPL_KICK(CLIENT(commander->get_nickname(), commander->get_username(), commander->get_IPaddr()), this->name, nickname, msg));
	server.send_response(RPL_KICK(CLIENT(commander->get_nickname(), commander->get_username(), commander->get_IPaddr()), this->name, nickname, msg), server.get_client(nickname)->get_fd());
}

void Channel::mode(Client *commander, int action, char const &mode)
{
	if (!get_op(commander))
	{
		server.send_response(ERR_CHANOPRIVSNEEDED(this->name), commander->get_fd());
		return;
	}
	if (action == ADD)
		add_mode(mode);
	else if (action == REMOVE)
		remove_mode(mode);
}

void Channel::op(Client *commander, int action, std::string const &nickname)
{
	if (!get_op(commander))
	{
		server.send_response(ERR_CHANOPRIVSNEEDED(this->name), commander->get_fd());
		return;
	}
	if (action == ADD)
	{
		if (get_op(nickname) == NULL)
		{
			Client *client = server.get_client(nickname);
			if (client == NULL)
			{
				server.send_response(ERR_NOSUCHNICK(nickname), commander->get_fd());
				return;
			}
			add_op(client);
			broadcast(RPL_YOUREOPER(CLIENT(commander->get_nickname(), commander->get_username(), commander->get_IPaddr()), this->name, nickname));
		}
	}
	else if (action == REMOVE)
	{
		if (server.get_client(nickname) == NULL)
		{
			server.send_response(ERR_NOSUCHNICK(nickname), commander->get_fd());
			return;
		}
		remove_op(nickname);
		broadcast(RPL_YOURENOTOPER(CLIENT(commander->get_nickname(), commander->get_username(), commander->get_IPaddr()), this->name, nickname));
	}
}

void Channel::topic(Client *commander)
{
	if (this->get_client(commander) == NULL)
	{
		server.send_response(ERR_NOTONCHANNEL(this->name), commander->get_fd());
		return;
	}
	if (this->get_topic().empty())
		server.send_response(RPL_NOTOPIC(CLIENT(commander->get_nickname(), commander->get_username(), commander->get_IPaddr()), this->get_channel_name()), commander->get_fd());
	else
		server.send_response(RPL_TOPIC(CLIENT(commander->get_nickname(), commander->get_username(), commander->get_IPaddr()), this->get_channel_name(), this->get_topic()), commander->get_fd());
}

void Channel::topic(Client *commander, int action, std::string const &topic)
{
	if (action == ADD)
	{
		if (get_client(commander) == NULL)
		{
			server.send_response(ERR_NOTONCHANNEL(this->name), commander->get_fd());
			return;
		}
		if (!get_op(commander))
		{
			std::cerr << "Client could not set topic: not an op" << std::endl;
			server.send_response(ERR_CHANOPRIVSNEEDED(this->name), commander->get_fd());
			return;
		}
		set_topic(topic);
		this->broadcast(RPL_TOPIC(CLIENT(commander->get_nickname(), commander->get_username(), commander->get_IPaddr()), this->name, this->get_topic()));
	}
	else if (action == REMOVE)
	{
		if (!get_op(commander))
		{
			std::cerr << "Client could not remove topic: not an op" << std::endl;
			server.send_response(ERR_CHANOPRIVSNEEDED(this->name), commander->get_fd());
			return;
		}
		set_topic("");
		this->broadcast(RPL_NOTOPIC(CLIENT(commander->get_nickname(), commander->get_username(), commander->get_IPaddr()), this->name));
	}
}

void Channel::quit(Client *client)
{
	std::cout << "Channel quit!" << std::endl;
	if (is_client_in_channel(client->get_nickname()) == false)
	{
		server.send_response(ERR_NOTONCHANNEL(this->name), client->get_fd());
		return;
	}
	broadcast(client, RPL_QUIT(CLIENT(client->get_nickname(), client->get_username(), client->get_IPaddr()), ""));
	remove_client(client);
	if (is_empty())
		server.remove_channel(this);
}
void Channel::quit(Client *client, std::string const &msg)
{
	std::cout << "Channel quit! msg" << std::endl;
	if (is_client_in_channel(client->get_nickname()) == false)
	{
		server.send_response(ERR_NOTONCHANNEL(this->name), client->get_fd());
		return;
	}
	broadcast(client, RPL_QUIT(CLIENT(client->get_nickname(), client->get_username(), client->get_IPaddr()), msg));
	remove_client(client);
	if (is_empty())
		server.remove_channel(this);
}

void Channel::message(Client *sender, std::string const &message)
{
	if (get_client(sender) == nullptr)
	{
		server.send_response(ERR_NOTONCHANNEL(this->name), sender->get_fd());
		return;
	}
	// Broadcasts to all exlude sender
	broadcast(sender, RPL_PRIVMSG(CLIENT(sender->get_nickname(), sender->get_username(), sender->get_IPaddr()), this->name, message));
}
