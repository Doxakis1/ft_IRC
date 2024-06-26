#include "Server.hpp"
#include "Message.hpp"

// PASS command
void Server::pass(std::string pass, int fd)
{
	Client *user = get_client(fd);
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
	std::string nick_in_use;
	size_t pos = nickname.find_first_not_of(" \t\v");
	if (pos != std::string::npos)
		nickname = nickname.substr(pos);
	Client *user = get_client(fd);
	if (pos == std::string::npos || nickname.empty())
	{
		this->send_response(ERR_NOTENOUGHPARAM(std::string("*")), fd);
		return;
	}
	if (nickname_in_use(nickname) && user->get_nickname() != nickname)
	{
		nick_in_use = "Changing to";
		if (user->get_nickname().empty())
			user->set_nickname(nick_in_use);
		this->send_response(ERR_NICKINUSE(this->name, nickname), fd);
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
				if (old_nick == nick_in_use && !user->get_username().empty())
				{
					user->set_logged_in(true);
					this->send_response(RPL_CONNECTED(user->get_nickname()), fd);
					this->send_response(RPL_NICKCHANGE(old_nick, user->get_nickname()), fd);
					return;
				}
				else if (!get_clients_channel(nickname).empty())
				{
					std::vector<std::string> clients_channels = get_clients_channel(nickname);
					for (auto it = clients_channels.begin(); it != clients_channels.end(); ++it)
						this->channels[*it]->broadcast(RPL_NICKCHANGECHANNEL(old_nick, user->get_username(), user->get_IPaddr(), nickname));
					return;
				}
				else
					this->send_response(RPL_NICKCHANGE(old_nick, user->get_nickname()), fd);
			}
		}
		if (user && user->is_registered() && !user->get_nickname().empty() && !user->get_username().empty() && user->get_nickname() != nick_in_use && !user->is_logged_in())
				this->send_response(RPL_CONNECTED(user->get_nickname()), fd);
	}
}

// USER command
void Server::username(std::vector<std::string> username, int fd)
{
	Client *user = get_client(fd);
	if (user && username.size() < 4)
	{
		this->send_response(ERR_NOTENOUGHPARAM(user->get_nickname()), fd);
		return ;
	}
	if (user && !user->get_username().empty())
	{
		this->send_response(ERR_ALREADYREGISTERED(user->get_nickname()), fd);
		return;
	}
	else
	{
		user->set_username(username[0]);
		user->set_hostname(username[2]);
		std::string realname = username[3].substr(1);
		user->set_realname(realname);
	}
	if (user && user->is_registered() && !user->get_nickname().empty() && !user->get_username().empty() && user->get_nickname() != "Changing to" && !user->is_logged_in())
				this->send_response(RPL_CONNECTED(user->get_nickname()), fd);
}

// JOIN command
void Server::join(Message &cmd, int fd)
{
	Client *user = get_client(fd);

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
		Channel *new_channel = new Channel(cmd.getParams().front(), user, *this);
		channels.insert(std::pair<std::string, Channel *>(cmd.getParams().front(), new_channel));
		user->add_channel(new_channel);
	}
	else
	{
		// add user to the channel
		if (cmd.getParams().size() > 1)
			channels[cmd.getParams().front()]->join(user, cmd.getParams()[1]);
		else
			channels[cmd.getParams().front()]->join(user, NO_KEY);
	}
}

void Server::quit(int fd)
{
	std::cout << RED << "Client <" << fd << "> Disconnected" << WHITE << std::endl;
	Client *client = get_client(fd);
	if (client->get_channels().size() > 0)
	{
		for (auto &channel : client->get_channels())
		{
			channel->quit(client);
		}
	}
	this->remove_client(fd);
	close(fd);
}

void Server::quit(Message &cmd, int fd)
{
	std::cout << RED << "Client <" << fd << "> Disconnected" << WHITE << std::endl;
	Client *client = get_client(fd);
	if (client->get_channels().size() > 0)
	{
		if (cmd.getParams().size() > 0 && cmd.getParams()[0][0] == ':')
		{
			std::string msg(cmd.getParams().front());
			for (auto &channel : client->get_channels())
			{
				channel->quit(client, msg);
			}
		}
		else
		{
			for (auto &channel : client->get_channels())
			{
				channel->quit(client);
			}
		}
	}
	this->remove_client(fd);
	close(fd);
}

void Server::privmsg(Message &cmd, int fd)
{
	Client *user = get_client(fd);
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
			this->send_response(ERR_NOSUCHNICK(user->get_nickname()), fd);
			return;
		}
		else
		{
			Client *recipient = get_client(cmd.getParams().front());
			this->send_response(RPL_PRIVMSG(CLIENT(user->get_nickname(), user->get_username(), user->get_IPaddr()), recipient->get_nickname(), cmd.getParams()[1]), recipient->get_fd());
		}
	}
}

void Server::mode(Message &cmd, int fd)
{
	Client *user = get_client(fd);
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
	if (cmd.getParams().front()[0] == '#')
	{
		// Channel mode
		if (channels.find(cmd.getParams().front()) == channels.end())
		{
			this->send_response(ERR_NOSUCHCHANNEL(cmd.getParams().front()), fd);
			return;
		}
		else
		{
			std::string mode = cmd.getParams()[1];
			if (cmd.getParams()[1][0] == '+')
			{
				if (cmd.getParams()[1][1] == 'o')
					channels[cmd.getParams().front()]->op(user, ADD, cmd.getParams()[2]);
				else
					channels[cmd.getParams().front()]->mode(user, ADD, cmd.getParams()[1][1]);
			}
			else if (cmd.getParams()[1][0] == '-')
			{
				if (cmd.getParams()[1][1] == 'o')
					channels[cmd.getParams().front()]->op(user, REMOVE, cmd.getParams()[2]);
				else
					channels[cmd.getParams().front()]->mode(user, REMOVE, cmd.getParams()[1][1]);
			}
		}
	}
}

void Server::invite(Message &cmd, int fd)
{
	Client *user = get_client(fd);
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
	if (channels.find(cmd.getParams()[1]) == channels.end())
	{
		this->send_response(ERR_NOSUCHCHANNEL(cmd.getParams().front()), fd);
		return;
	}
	channels[cmd.getParams()[1]]->invite(user, cmd.getParams()[0]);
}

void Server::topic(Message &cmd, int fd)
{
	Client *user = get_client(fd);
	if (!user->is_registered())
	{
		this->send_response(ERR_NOTREGISTERED(this->get_name()), fd);
		return;
	}
	if (cmd.getParams().size() < 1)
	{
		this->send_response(ERR_NOTENOUGHPARAM(user->get_nickname()), fd);
		return;
	}
	if (channels.find(cmd.getParams().front()) == channels.end())
	{
		this->send_response(ERR_NOSUCHCHANNEL(cmd.getParams().front()), fd);
		return;
	}
	if (cmd.getParams().size() == 1)
		channels[cmd.getParams().front()]->topic(user);
	else
	{
		if (cmd.getParams()[1].size() == 1)
			channels[cmd.getParams().front()]->topic(user, REMOVE, cmd.getParams()[1]);
		else
			channels[cmd.getParams().front()]->topic(user, ADD, cmd.getParams()[1]);
	}
}

void Server::kick(Message &cmd, int fd)
{
	Client *user = get_client(fd);
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
	if (channels.find(cmd.getParams().front()) == channels.end())
	{
		this->send_response(ERR_NOSUCHCHANNEL(cmd.getParams().front()), fd);
		return;
	}
	Channel *kick_ch = channels[cmd.getParams().front()];
	if (cmd.getParams().size() > 2)
	{
		channels[cmd.getParams().front()]->kick(user, cmd.getParams()[1], cmd.getParams()[2]);
	}
	else
	{
		channels[cmd.getParams().front()]->kick(user, cmd.getParams()[1]);
	}
	this->get_client(cmd.getParams()[1])->remove_channel(kick_ch);
	if (this->channels[cmd.getParams().front()]->is_empty())
		this->remove_channel(this->channels[cmd.getParams().front()]);
}
