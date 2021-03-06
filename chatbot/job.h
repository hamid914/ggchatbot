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

#ifndef JOB_H
#define JOB_H

#include <QObject>
#include <QPointer>
#include <QThread>

class Job : public QObject
{
    Q_OBJECT
public:
    Job();
    ~Job();

    int timerPeriod();
    void setTimerPeriod(int p);
    void setJobThread(QSharedPointer<QThread>);
    void destroyJob();

    virtual void makeJob() = 0;

public slots:
    void runJob();

private:
    int period;
    QSharedPointer<QThread> jobThreadPtr;
};

typedef QPointer<Job> JobPtr;

#endif // JOB_H
