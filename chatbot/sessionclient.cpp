/*
    This file is part of GGChatBot.

    Copyright (C) 2009  Dariusz Mikulski <dariusz.mikulski@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include "sessionclient.h"
#include "botsettingsto.h"
#include "sessionscheduler.h"
#include "userdatabase.h"
#include "profile.h"
#include "imagedescriptionsettings.h"

#include <errno.h>
#include <QCoreApplication>
#include <QBuffer>
#include <QXmlQuery>

const char * BOT_DEFAULT_VERSION = "Gadu-Gadu Client Build 10.0.0.11070";

SessionClient::SessionClient(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<uin_t>("uin_t");
    QObject::connect(&eventManager, SIGNAL(sendMessage(QString)), this, SLOT(sendMessage(QString)));
    QObject::connect(&eventManager, SIGNAL(sendMessage(uin_t, QString)), this, SLOT(sendMessage(uin_t, QString)));
    QObject::connect(&eventManager, SIGNAL(sendMessageTo(uin_t, QString)), this, SLOT(sendMessageTo(uin_t, QString)));
    QObject::connect(&eventManager, SIGNAL(sendMessageRichtext(uin_t, QString, const unsigned char*, int)), this, SLOT(sendMessageRichtext(uin_t, QString, const unsigned char*, int)));
    QObject::connect(&eventManager, SIGNAL(sendMessageRichtextTo(uin_t, QString, const unsigned char*, int)), this, SLOT(sendMessageRichtextTo(uin_t, QString, const unsigned char*, int)));
}

SessionClient::~SessionClient()
{
}

void SessionClient::MakeConnection()
{
    eventManager.SetProfile(GetProfile());

    SetDebugLevel();

    if(!Login())
    {
        return;
    }

    EventLoop();
}

void SessionClient::FreeSession(gg_session *session)
{
    if(session)
    {
        gg_free_event(event);
        gg_free_session(session);
    }
}

void SessionClient::Logout(gg_session *session)
{
    qDebug() << "Bot logout.";
    gg_logoff(session);
}

void SessionClient::CleanAndExit()
{
    Logout(session);
    FreeSession(session);

    qDebug() << "Bye, bye :)";
    //emit endServer();
}

void SessionClient::SetDebugLevel()
{
    qDebug() << "SetDebugLevel() called";

    gg_debug_level = GetProfile()->getBotSettings().getDebugLevel();
}

bool SessionClient::Login()
{
    qDebug() << "Logowanie...";
    memset(&loginParams, 0, sizeof(loginParams));

    uin_t uin = GetProfile()->getBotSettings().getUin();
    qDebug() << "UIN:" << uin;

    if(uin == 0)
    {
        qDebug() << "Skonfiguruj swoj UIN!";
        endServer();
        return false;
    }

    loginParams.uin = uin;
    loginParams.password = GetProfile()->getBotSettings().getPassword().toLatin1().data();
    loginParams.async = 1;
    loginParams.status = GG_STATUS_FFC;
    loginParams.protocol_version = GG_DEFAULT_PROTOCOL_VERSION;
    loginParams.client_version = const_cast<char*>(BOT_DEFAULT_VERSION);
    loginParams.has_audio = 0;
    loginParams.encoding = GG_ENCODING_UTF8;
    loginParams.protocol_features = (GG_FEATURE_STATUS80BETA|GG_FEATURE_MSG80|GG_FEATURE_STATUS80|GG_FEATURE_MSG77|GG_FEATURE_STATUS77|GG_FEATURE_DND_FFC|GG_FEATURE_IMAGE_DESCR);

    if (!( session = gg_login(&loginParams) ) )
    {
        qDebug() << "Nie udalo sie polaczyc.";
        emit endServer();
        return false;
    }

    qDebug() << "Laczenie...";

    qDebug() << "Protocol version:" << session->protocol_version << "client version:" << session->client_version;

    scheduler.clearJobs();
    pingServer = JobPtr(new PingServerJob(session));
    KickUserJob *k = new KickUserJob();
    k->SetProfile(GetProfile());
    kickUser = JobPtr(k);
    scheduler.addJob(pingServer);
    scheduler.addJob(kickUser);
    
    return true;
}

bool SessionClient::SendContactList()
{
    if(gg_notify(session, NULL, 0) == -1)
    {
        qDebug() << "Blad wysylania listy kontaktow na serwer";
        return false;
    }

    qDebug() << "Lista kontaktow wyslana na serwer.";
    return true;
}

void SessionClient::ChangeStatus(QString description, int status)
{
    qDebug() << "Zmieniam status na: " << description;
    description = GGChatBot::makeMessage(description);
    QByteArray data = description.toUtf8();
    gg_change_status_descr(session, status, data.data());
}

void SessionClient::SetImageStatus(QString description)
{
    qDebug() << "Zmieniam status na status obrazkowy" << description.toUtf8();
    //QByteArray data = GGChatBot::unicode2cp(GGChatBot::makeMessage(description));
    QByteArray data = description.toUtf8();
    ChangeStatus(description, GG_STATUS_FFC);
    gg_change_status_descr(session, GG_STATUS_DESCR_MASK | GG_STATUS_IMAGE_MASK | GG_STATUS_FFC, data.data());
}

bool SessionClient::WaitForEvent()
{
    if( !(event = gg_watch_fd(session)) )
        return false;

    return true;
}

void SessionClient::EventLoop()
{
    forever
    {
        QCoreApplication::instance()->processEvents();

        FD_ZERO(&rd);
        FD_ZERO(&wd);

        if ((session->check & GG_CHECK_READ))
            FD_SET(session->fd, &rd);

        if ((session->check & GG_CHECK_WRITE))
            FD_SET(session->fd, &wd);

        if (session->timeout)
        {
            tv.tv_sec = 0;//session->timeout;
            tv.tv_usec = 10;
        }

        int wynik = select(session->fd + 1, &rd, &wd, NULL, /*(session->timeout) ? &tv : NULL*/ &tv);

        if (wynik == -1)
        {
            if (errno != EINTR)
            {
                qDebug() << "Blad funkcji select(): " << strerror(errno);
                emit endServer();
                return;
            }
        }

        if (FD_ISSET(session->fd, &rd) || FD_ISSET(session->fd, &wd))
        {
            if(!WaitForEvent())
            {
                qDebug() << "Rozlaczono z serwerem, ponawiam...";

                gg_event_free(event);
                gg_logoff(session);
                gg_free_session(session);

                QTimer::singleShot(5000, this, SLOT(MakeConnection()));
                return;
            }

            if(event->type == GG_EVENT_NONE)
            {
                gg_event_free(event);
                continue;
            }

            if(event->type == GG_EVENT_CONN_SUCCESS)
            {
                if(!SendContactList())
                {
                    CleanAndExit();
                    return;
                }

                ChangeStatus(GetProfile()->getBotSettings().getDefaultDescription());
            }

            if(event->type == GG_EVENT_CONN_FAILED)
            {
                ShowFailureReason(event);
                qDebug() << "Connection failed. Ponawiam.";
                //emit restartConnection();
                //emit endServer();

                gg_event_free(event);
                gg_logoff(session);
                gg_free_session(session);

                QTimer::singleShot(5000, this, SLOT(MakeConnection()));
                return;
            }

            if(event->type == GG_XML_EVENT || event->type == GG_EVENT_XML_EVENT)
            {
                ReadImageStatus(event);
            }

            eventManager.ResolveEvent(event);
            
            gg_event_free(event);
        }
    }
}

void SessionClient::ShowFailureReason(struct gg_event *event)
{
    switch(event->event.failure)
    {
    case GG_FAILURE_RESOLVING:
        qDebug() << "Resolving server failed.";
        break;

    case GG_FAILURE_CONNECTING:
        qDebug() << "Connection problem.";
        break;

    case GG_FAILURE_INVALID:
        qDebug() << "Server returned failed data.";
        break;

    case GG_FAILURE_READING:
        qDebug() << "Error while reading.";
        break;

    case GG_FAILURE_WRITING:
        qDebug() << "Error while writing.";
        break;

    case GG_FAILURE_PASSWORD:
        qDebug() << "Password incorrect.";
        break;

    case GG_FAILURE_404:
        qDebug() << "GG_FAILURE_404";
        break;

    case GG_FAILURE_TLS:
        qDebug() << "Error while tls negotiation.";
        break;

    case GG_FAILURE_NEED_EMAIL:
        qDebug() << "Disconnected by server. Please change email address.";
        break;

    case GG_FAILURE_INTRUDER:
        qDebug() << "Too much connections with wrong password";
        break;

    case GG_FAILURE_UNAVAILABLE:
        qDebug() << "Server is turned off.";
        break;

    default:
        qDebug() << "Error:" << event->event.failure;
    }
}

void SessionClient::ReadImageStatus(struct gg_event *event)
{
    QString xmlEvent = QString::fromUtf8((const char *)event->event.xml_event.data);

    qDebug() << "New XML Event: " << xmlEvent;

    QString xmlUserbarId("doc($internalFile)/activeUserbarEventList/activeUserbarEvent/userbarId/string()");
    QString xmlBeginTime("doc($internalFile)/activeUserbarEventList/activeUserbarEvent/beginTime/string()");
    QString xmlExpireTime("doc($internalFile)/activeUserbarEventList/activeUserbarEvent/expireTime/string()");

    QByteArray out = xmlEvent.toLatin1();
    QBuffer outputBuffer(&out);
    outputBuffer.open(QIODevice::ReadOnly);

    QXmlQuery query;
    query.bindVariable("internalFile", &outputBuffer);

    QStringList userbarId;
    query.setQuery(xmlUserbarId);
    query.evaluateTo(&userbarId);

    QStringList beginTime;
    query.setQuery(xmlBeginTime);
    query.evaluateTo(&beginTime);

    QStringList expireTime;
    query.setQuery(xmlExpireTime);
    query.evaluateTo(&expireTime);

    ImageDescriptionSettings imageDescSettings;
    QList<ImageDescription> idescList;
    imageDescSettings.readImageDescSettings(idescList);

    for(int i=0; i<userbarId.size(); ++i)
    {
        bool bFound = false;
        foreach(ImageDescription imgDescription, idescList)
        {
            if(imgDescription.userbarId == userbarId.at(i))
            {
                bFound = true;
                break;
            }
        }

        if(bFound)
            continue;

        ImageDescription idesc;
        idesc.userbarId = userbarId.at(i);
        QString btime = beginTime.at(i).left(beginTime.at(i).length() - (beginTime.at(i).length()-beginTime.at(i).indexOf("+")));
        QString etime = expireTime.at(i).left(expireTime.at(i).length() - (expireTime.at(i).length()-expireTime.at(i).indexOf("+")));
        idesc.beginTime = QDateTime::fromString(btime, Qt::ISODate);
        idesc.expireTime = QDateTime::fromString(etime, Qt::ISODate);

        idescList.push_back(idesc);
    }

    imageDescSettings.saveImageDescription(idescList);
}

void SessionClient::sendMessage(QString message)
{
    if(!session)
        return;

    message = GGChatBot::makeMessage(message);
    QByteArray data = message.toUtf8();

    QList<UserInfoTOPtr> users = GetProfile()->getUserDatabase()->getUserList();
    foreach(UserInfoTOPtr user, users)
    {
        if(GetProfile()->getUserDatabase()->isUserOnChannel(user))
            gg_send_message(session, GG_CLASS_MSG, user->getUin(), (const unsigned char*)data.constData());
    }
}

void SessionClient::sendMessageRichtext(uin_t uin, QString message, const unsigned char* format, int formatlen)
{
    if(!session || !canUserWriteToChannel(uin))
        return;

    message = GGChatBot::makeMessage(message);
    QByteArray data = message.toUtf8();

    QList<UserInfoTOPtr> users = GetProfile()->getUserDatabase()->getUserList();
    foreach(UserInfoTOPtr user, users)
    {
        if(GGChatBot::DISABLE_BACK_MESSAGE)
        {
            if(user->getUin() != uin && GetProfile()->getUserDatabase()->isUserOnChannel(user))
                gg_send_message_richtext(session, GG_CLASS_MSG, user->getUin(), (const unsigned char*)data.constData(), format, formatlen);
        }
        else
            gg_send_message_richtext(session, GG_CLASS_MSG, user->getUin(), (const unsigned char*)data.constData(), format, formatlen);
    }
}

void SessionClient::sendMessageRichtextTo(uin_t uin, QString message, const unsigned char *format, int formatlen)
{
    if(!session)
	return;

    QByteArray data = message.toUtf8();

    gg_send_message_richtext(session, GG_CLASS_MSG, uin, (const unsigned char*)data.constData(), format, formatlen);
}

void SessionClient::sendMessage(uin_t uin, QString message)
{
    if(!session || !canUserWriteToChannel(uin))
        return;

    QList<UserInfoTOPtr> users = GetProfile()->getUserDatabase()->getUserList();
    foreach(UserInfoTOPtr user, users)
    {
        if(GGChatBot::DISABLE_BACK_MESSAGE)
        {
            if(user->getUin() != uin && GetProfile()->getUserDatabase()->isUserOnChannel(user))
                sendMessageTo(user->getUin(), message);
        }
        else
            sendMessageTo(user->getUin(), message);    
    }
}

void SessionClient::sendMessageTo(uin_t uin, QString message)
{
    if(!session)
        return;
   
    message = GGChatBot::makeMessage(message);

    QByteArray data = message.toUtf8();
    gg_send_message(session, GG_CLASS_MSG, uin, (const unsigned char*)data.constData());
}

void SessionClient::sendMessageToSuperUser(uin_t uin, QString message)
{
    QList<UserInfoTOPtr> users = GetProfile()->getUserDatabase()->getUserList();
    foreach(UserInfoTOPtr user, users)
    {
        if(user->getUin() == uin
           && GetProfile()->getUserDatabase()->isSuperUser(user)
           && GetProfile()->getUserDatabase()->isUserOnChannel(user))
            sendMessageTo(user->getUin(), message);
    }
}

void SessionClient::sendMessageToStaff(QString message)
{
    QList<UserInfoTOPtr> users = GetProfile()->getUserDatabase()->getUserList();
    UserDatabasePtr ud = GetProfile()->getUserDatabase();

    foreach(UserInfoTOPtr user, users)
    {
        if((ud->isSuperUser(user) || ud->isUserHaveOp(user)) && ud->isUserOnChannel(user))
        {
            sendMessageTo(user->getUin(), message);
        }
    }
}

bool SessionClient::canUserWriteToChannel(uin_t uin)
{
    bool channelModerated = GetProfile()->getBotSettings().getChannelModerated();
    if(!channelModerated)
        return true;

    UserInfoTOPtr user = GetProfile()->getUserDatabase()->getUserInfo(uin);
    if(user->getUserFlags() >= GGChatBot::VOICE_USER_FLAG)
        return true;

    return false;
}

