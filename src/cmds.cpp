#include "Server.hpp"

// PASS command
void Server::pass(std::string pass, int fd)
{
	std::shared_ptr<Client> user = get_client(fd);
	size_t pos = pass.find_first_not_of(" \t\v");
	if (pos == std::string::npos || pass.empty())
		this->send_response(ERR_NOTENOUGHPARAM(std::string("*")), fd);
	else if (!user->is_registered())
		if (pass == this->password)
			user->set_registered(true);
		else
			this->send_response(ERR_INCORPASS(std::string("*")), fd);
	else
		this->send_response(ERR_ALREADYREGISTERED(user->get_nickname()), fd);
}
// NICK command
void Server::nick(std::string nickname, int fd)
{
	size_t pos = nickname.find_first_not_of(" \t\v");
	if (pos != std::string::npos)
		nickname = nickname.substr(pos);
	std::shared_ptr<Client> user = get_client(fd);
	if (pos == std::string::npos || nickname.empty())
	{
		this->send_response(ERR_NOTENOUGHPARAM(std::string("*")), fd);
		return;
	}
	if (nickname_in_use(nickname) && user->get_nickname() != nickname)
	{
		std::string nick_in_use = "!";
		if (user->get_nickname().empty())
			user->set_nickname(nick_in_use);
		this->send_response(ERR_NICKINUSE(nickname), fd);
		return;
	}
	if (!is_valid_nickname(nickname))
	{
		this->send_response(ERR_ERRONEUSNICK(nickname), fd);
		return;
	}
	else
	{
		if (user && user->is_registered())
		{
			std::string old_nick = user->get_nickname();
			user->set_nickname(nickname);
			if (!old_nick.empty() && old_nick != nickname)
			{
				if (old_nick == "!" && !user->get_username().empty())
				{
					user->set_registered(true);
					this->send_response(RPL_CONNECTED(user->get_nickname()), fd);
					this->send_response(RPL_NICKCHANGE(user->get_nickname(), nickname), fd);
				}
				else if (!get_clients_channel(nickname).empty())
				{
					std::vector<std::string> clients_channels = get_clients_channel(nickname);
					for (auto it = clients_channels.begin(); it != clients_channels.end(); ++it)
						this->channels[*it]->broadcast(RPL_PRIVMSG(CLIENT(old_nick, user->get_username(), user->get_IPaddr()), channels[*it]->get_channel_name(), old_nick + " is now knows as " + nickname));
				}
				else
					this->send_response(RPL_NICKCHANGE(old_nick, nickname), fd);
				return;
			}
		}
	}
	if (user && user->is_registered() && !user->get_nickname().empty() && !user->get_username().empty())
		this->send_response(RPL_CONNECTED(user->get_nickname()), fd);
}

// USER command
void Server::username(std::vector<std::string> username, int fd)
{
	std::shared_ptr<Client> user = get_client(fd);
	if (user && username.size() < 4)
	{
		this->send_response(ERR_NOTENOUGHPARAM(user->get_nickname()), fd);
	}
	if (user && !user->get_username().empty())
	{
		this->send_response(ERR_ALREADYREGISTERED(user->get_nickname()), fd);
		return;
	}
	else
		user->set_username(username[0]);
	if (user && user->is_registered() && !user->get_username().empty() && !user->get_nickname().empty())
		this->send_response(RPL_CONNECTED(user->get_nickname()), fd);
}

// JOIN command
void Server::join(Message &cmd, int fd)
{
	std::shared_ptr<Client> user = get_client(fd);

	if (!user->is_registered())
	{
		this->send_response(ERR_NOTREGISTERED(this->get_name()), fd);
		return;
	}

	// check if JOIN command has enough parameters
	if (cmd.getParams().size() == 0)
	{
		this->send_response(ERR_NOTENOUGHPARAM(user->get_nickname()), fd);
		return;
	}
	// check if channel exists and if not create it
	if (channels.find(cmd.getParams().front()) == channels.end())
	{
		std::shared_ptr<Channel> new_channel = std::make_shared<Channel>(cmd.getParams().front(), user, *this);
		channels.insert(std::pair<std::string, std::shared_ptr<Channel>>(cmd.getParams().front(), new_channel));
	}
	else
	{
		// add user to the channel
		if (cmd.getParams().size() > 1)
			channels[cmd.getParams().front()]->join(user, cmd.getParams()[1]);
		else
			channels[cmd.getParams().front()]->join(user, NO_KEY);
		// print all users in the channel
		// for (auto &client : channels[cmd.getParams().front()].get_clients())
		// {
		// 	std::cout << client.first << std::endl;
		// }
	}
}

void Server::quit(int fd)
{
	std::cout << RED << "Client <" << fd << "> Disconnected" << WHITE << std::endl;
	this->remove_client(fd);
	close(fd);
}

void Server::privmsg(Message &cmd, int fd)
{
	std::shared_ptr<Client> user = get_client(fd);
	if (!user->is_registered())
	{
		this->send_response(ERR_NOTREGISTERED(this->get_name()), fd);
		return;
	}
	if (cmd.getParams().size() < 2)
	{
		this->send_response(ERR_NOTENOUGHPARAM(user->get_nickname()), fd);
		return;
	}
	if (cmd.getParams().front()[0] == '#') // if the first parameter is a channel
	{
		if (channels.find(cmd.getParams().front()) == channels.end())
		{
			this->send_response(ERR_NOSUCHCHANNEL(cmd.getParams().front()), fd);
			return;
		}
		else
			channels[cmd.getParams().front()]->message(user, cmd.getParams()[1]);
	}
	else
	{
		if (get_client(cmd.getParams().front()) == NULL)
		{
			this->send_response(ERR_NOSUCHNICK(user->get_nickname(), cmd.getParams().front()), fd);
			return;
		}
		else
		{
			std::shared_ptr<Client> recipient = get_client(cmd.getParams().front());
			this->send_response(RPL_PRIVMSG(CLIENT(user->get_nickname(), user->get_username(), user->get_IPaddr()), recipient->get_nickname(), cmd.getParams()[1]), recipient->get_fd());
		}
	}
}
