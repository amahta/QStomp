/*
 * This file is part of QStomp
 *
 * Copyright (C) 2009 Patrick Schneider <patrick.p2k.schneider@googlemail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "qstomp.h"
#include "qstomp_p.h"

#include <QtCore/QStringList>
#include <QtCore/QSet>
#include <QtCore/QTextCodec>
#include <QtNetwork/QTcpSocket>

static const QList<QByteArray> VALID_COMMANDS = QList<QByteArray>() << "ABORT" << "ACK" << "BEGIN" << "COMMIT" << "CONNECT" << "DISCONNECT"
												<< "CONNECTED" << "MESSAGE" << "SEND" << "SUBSCRIBE" << "UNSUBSCRIBE" << "RECEIPT" << "ERROR";

QStompFrame::QStompFrame() : d(new QStompFramePrivate)
{
	d->m_valid = true;
	d->m_textCodec = QTextCodec::codecForName("utf-8");
}

QStompFrame::QStompFrame(const QStompFrame &other) : d(new QStompFramePrivate)
{
	d->m_valid = other.d->m_valid;
	d->m_header = other.d->m_header;
	d->m_body = other.d->m_body;
	d->m_textCodec = other.d->m_textCodec;
}

QStompFrame::QStompFrame(const QByteArray &frame) : d(new QStompFramePrivate)
{
	d->m_textCodec = QTextCodec::codecForName("utf-8");
	d->m_valid = this->parse(frame);
}

QStompFrame::~QStompFrame()
{
	delete this->d;
}

QStompFrame & QStompFrame::operator=(const QStompFrame &other)
{
	d->m_valid = other.d->m_valid;
	d->m_header = other.d->m_header;
	d->m_body = other.d->m_body;
	d->m_textCodec = other.d->m_textCodec;
	return *this;
}

void QStompFrame::setHeaderValue(const QByteArray &key, const QByteArray &value)
{
	QByteArray lowercaseKey = key.toLower();
	QStompHeaderList::Iterator it = d->m_header.begin();
	while (it != d->m_header.end()) {
		if ((*it).first.toLower() == lowercaseKey) {
			(*it).second = value;
			return;
		}
		++it;
	}
	this->addHeaderValue(key, value);
}

void QStompFrame::setHeaderValues(const QStompHeaderList &values)
{
	d->m_header = values;
}

void QStompFrame::addHeaderValue(const QByteArray &key, const QByteArray &value)
{
	d->m_header.append(qMakePair(key, value));
}

QStompHeaderList QStompFrame::header() const
{
	return d->m_header;
}

bool QStompFrame::headerHasKey(const QByteArray &key) const
{
	QByteArray lowercaseKey = key.toLower();
	QStompHeaderList::ConstIterator it = d->m_header.constBegin();
	while (it != d->m_header.constEnd()) {
		if ((*it).first.toLower() == lowercaseKey)
			return true;
		++it;
	}
	return false;
}

QList<QByteArray> QStompFrame::headerKeys() const
{
	QList<QByteArray> keyList;
	QSet<QByteArray> seenKeys;
	QStompHeaderList::ConstIterator it = d->m_header.constBegin();
	while (it != d->m_header.constEnd()) {
		QByteArray key = (*it).first;
		QByteArray lowercaseKey = key.toLower();
		if (!seenKeys.contains(lowercaseKey)) {
			keyList.append(key);
			seenKeys.insert(lowercaseKey);
		}
		++it;
	}
	return keyList;
}

QByteArray QStompFrame::headerValue(const QByteArray &key) const
{
	QByteArray lowercaseKey = key.toLower();
	QStompHeaderList::ConstIterator it = d->m_header.constBegin();
	while (it != d->m_header.constEnd()) {
		if ((*it).first.toLower() == lowercaseKey)
			return (*it).second;
		++it;
	}
	return QByteArray();
}

QList<QByteArray> QStompFrame::allHeaderValues(const QByteArray &key) const
{
	QList<QByteArray> valueList;
	QByteArray lowercaseKey = key.toLower();
	QStompHeaderList::ConstIterator it = d->m_header.constBegin();
	while (it != d->m_header.constEnd()) {
		if ((*it).first.toLower() == lowercaseKey)
			valueList.append((*it).second);
		++it;
	}
	return valueList;
}

void QStompFrame::removeHeaderValue(const QByteArray &key)
{
	QByteArray lowercaseKey = key.toLower();
	QStompHeaderList::Iterator it = d->m_header.begin();
	while (it != d->m_header.end()) {
		if ((*it).first.toLower() == lowercaseKey) {
			d->m_header.erase(it);
			return;
		}
		++it;
	}
}

void QStompFrame::removeAllHeaderValues(const QByteArray &key)
{
	QByteArray lowercaseKey = key.toLower();
	QStompHeaderList::Iterator it = d->m_header.begin();
	while (it != d->m_header.end()) {
		if ((*it).first.toLower() == lowercaseKey) {
			d->m_header.erase(it);
			continue;
		}
		++it;
	}
}

bool QStompFrame::hasContentLength() const
{
	return this->headerHasKey("content-length");
}

uint QStompFrame::contentLength() const
{
	return this->headerValue("content-length").toUInt();
}

void QStompFrame::setContentLength(uint len)
{
	this->setHeaderValue("content-length", QByteArray::number(len));
}

bool QStompFrame::hasContentType() const
{
	return this->headerHasKey("content-type");
}

QByteArray QStompFrame::contentType() const
{
	QByteArray type = this->headerValue("content-type");
	if (type.isEmpty())
		return QByteArray();

	int pos = type.indexOf(';');
	if (pos == -1)
		return type;

	return type.left(pos).trimmed();
}

void QStompFrame::setContentType(const QByteArray &type)
{
	this->setHeaderValue("content-type", type);
}

bool QStompFrame::hasContentEncoding() const
{
	return this->headerHasKey("content-length");
}

QByteArray QStompFrame::contentEncoding() const
{
	return this->headerValue("content-encoding");
}

void QStompFrame::setContentEncoding(const QByteArray &name)
{
	this->setHeaderValue("content-encoding", name);
	d->m_textCodec = QTextCodec::codecForName(name);
}

void QStompFrame::setContentEncoding(const QTextCodec * codec)
{
	this->setHeaderValue("content-encoding", codec->name());
	d->m_textCodec = codec;
}

QByteArray QStompFrame::toByteArray() const
{
	if (!this->isValid())
		return QByteArray("");

	QByteArray ret = QByteArray("");

	QStompHeaderList::ConstIterator it = d->m_header.constBegin();
	while (it != d->m_header.constEnd()) {
		QByteArray key = (*it).first;
		if (key.toLower() != "login" && key.toLower() != "passcode")
			ret += key + ": " + (*it).second + "\n";
		else
			ret += key + ":" + (*it).second + "\n";
		++it;
	}
	ret.append('\n');
	return ret + d->m_body;
}

bool QStompFrame::isValid() const
{
	return d->m_valid;
}

bool QStompFrame::parseHeaderLine(const QByteArray &line, int)
{
	int i = line.indexOf(':');
	if (i == -1)
		return false;

	QByteArray key = line.left(i).trimmed();
	if (key.toLower() != "passcode" && key.toLower() != "login")
		this->addHeaderValue(key, line.mid(i + 1).trimmed());
	else
		this->addHeaderValue(key, line.mid(i + 1));

	return true;
}

bool QStompFrame::parse(const QByteArray &frame)
{
	int headerEnd = frame.indexOf("\n\n");
	if (headerEnd == -1)
		return false;

	d->m_body = frame.mid(headerEnd+2);

	QList<QByteArray> lines = frame.left(headerEnd).split('\n');

	if (lines.isEmpty())
		return false;

	for (int i = 0; i < lines.size(); i++) {
		if (!this->parseHeaderLine(lines.at(i), i))
			return false;
	}
	if (this->hasContentLength())
		d->m_body.resize(this->contentLength());
	else if (d->m_body.endsWith(QByteArray("\0\n", 2)))
		d->m_body.chop(2);

	return true;
}

void QStompFrame::setValid(bool v)
{
	d->m_valid = v;
}

QString QStompFrame::body() const
{
	return d->m_textCodec->toUnicode(d->m_body);
}

QByteArray QStompFrame::rawBody() const
{
	return d->m_body;
}

void QStompFrame::setBody(const QString &body)
{
	d->m_body = d->m_textCodec->fromUnicode(body);
}

void QStompFrame::setRawBody(const QByteArray &body)
{
	d->m_body = body;
}


QStompResponseFrame::QStompResponseFrame() : QStompFrame(), dd(new QStompResponseFramePrivate)
{
	this->setType(QStompResponseFrame::ResponseInvalid);
}

QStompResponseFrame::QStompResponseFrame(const QStompResponseFrame &other) : QStompFrame(other), dd(new QStompResponseFramePrivate)
{
	dd->m_type = other.dd->m_type;
}

QStompResponseFrame::QStompResponseFrame(const QByteArray &frame) : QStompFrame(), dd(new QStompResponseFramePrivate)
{
	this->setValid(this->parse(frame));
}

QStompResponseFrame::QStompResponseFrame(QStompResponseFrame::ResponseType type) : QStompFrame(), dd(new QStompResponseFramePrivate)
{
	this->setType(type);
}

QStompResponseFrame::~QStompResponseFrame()
{
	delete this->dd;
}

QStompResponseFrame & QStompResponseFrame::operator=(const QStompResponseFrame &other)
{
	QStompFrame::operator=(other);
	dd->m_type = other.dd->m_type;
	return *this;
}

void QStompResponseFrame::setType(QStompResponseFrame::ResponseType type)
{
	this->setValid(type != QStompResponseFrame::ResponseInvalid);
	dd->m_type = type;
}

QStompResponseFrame::ResponseType QStompResponseFrame::type()
{
	return dd->m_type;
}

bool QStompResponseFrame::parseHeaderLine(const QByteArray &line, int number)
{
	if (number != 0)
		return QStompFrame::parseHeaderLine(line, number);

	if (line == "CONNECTED")
		dd->m_type = QStompResponseFrame::ResponseConnected;
	else if (line == "MESSAGE")
		dd->m_type = QStompResponseFrame::ResponseMessage;
	else if (line == "RECEIPT")
		dd->m_type = QStompResponseFrame::ResponseReceipt;
	else if (line == "ERROR")
		dd->m_type = QStompResponseFrame::ResponseError;
	else
		return false;

	return true;
}

QByteArray QStompResponseFrame::toByteArray() const
{
	if (!this->isValid())
		return QByteArray("");

	QByteArray ret;
	switch (dd->m_type) {
		case QStompResponseFrame::ResponseInvalid:
			return QByteArray("");
		case QStompResponseFrame::ResponseConnected:
			ret = "CONNECTED\n"; break;
		case QStompResponseFrame::ResponseMessage:
			ret = "MESSAGE\n"; break;
		case QStompResponseFrame::ResponseReceipt:
			ret = "RECEIPT\n"; break;
		case QStompResponseFrame::ResponseError:
			ret = "ERROR\n"; break;
	}
	return ret + QStompFrame::toByteArray();
}

bool QStompResponseFrame::hasDestination() const
{
	return this->headerHasKey("destination");
}

QByteArray QStompResponseFrame::destination() const
{
	return this->headerValue("destination");
}

void QStompResponseFrame::setDestination(const QByteArray &value)
{
	this->setHeaderValue("destination", value);
}

bool QStompResponseFrame::hasSubscriptionId() const
{
	return this->headerHasKey("subscription");
}

QByteArray QStompResponseFrame::subscriptionId() const
{
	return this->headerValue("subscription");
}

void QStompResponseFrame::setSubscriptionId(const QByteArray &value)
{
	this->setHeaderValue("subscription", value);
}

bool QStompResponseFrame::hasMessageId() const
{
	return this->headerHasKey("message-id");
}

QByteArray QStompResponseFrame::messageId() const
{
	return this->headerValue("message-id");
}

void QStompResponseFrame::setMessageId(const QByteArray &value)
{
	this->setHeaderValue("message-id", value);
}

bool QStompResponseFrame::hasReceiptId() const
{
	return this->headerHasKey("receipt-id");
}

QByteArray QStompResponseFrame::receiptId() const
{
	return this->headerValue("receipt-id");
}

void QStompResponseFrame::setReceiptId(const QByteArray &value)
{
	this->setHeaderValue("receipt-id", value);
}

bool QStompResponseFrame::hasMessage() const
{
	return this->headerHasKey("message");
}

QByteArray QStompResponseFrame::message() const
{
	return this->headerValue("message");
}

void QStompResponseFrame::setMessage(const QByteArray &value)
{
	this->setHeaderValue("message", value);
}


QStompRequestFrame::QStompRequestFrame() : QStompFrame(), dd(new QStompRequestFramePrivate)
{
	this->setType(QStompRequestFrame::RequestInvalid);
}

QStompRequestFrame::QStompRequestFrame(const QStompRequestFrame &other) : QStompFrame(other), dd(new QStompRequestFramePrivate)
{
	dd->m_type = other.dd->m_type;
}

QStompRequestFrame::QStompRequestFrame(const QByteArray &frame) : QStompFrame(), dd(new QStompRequestFramePrivate)
{
	this->setValid(this->parse(frame));
}

QStompRequestFrame::QStompRequestFrame(QStompRequestFrame::RequestType type) : QStompFrame(), dd(new QStompRequestFramePrivate)
{
	this->setType(type);
}

QStompRequestFrame::~QStompRequestFrame()
{
	delete this->dd;
}

QStompRequestFrame & QStompRequestFrame::operator=(const QStompRequestFrame &other)
{
	QStompFrame::operator=(other);
	dd->m_type = other.dd->m_type;
	return *this;
}

void QStompRequestFrame::setType(QStompRequestFrame::RequestType type)
{
	this->setValid(type != QStompRequestFrame::RequestInvalid);
	dd->m_type = type;
}

QStompRequestFrame::RequestType QStompRequestFrame::type()
{
	return dd->m_type;
}

bool QStompRequestFrame::parseHeaderLine(const QByteArray &line, int number)
{
	if (number != 0)
		return QStompFrame::parseHeaderLine(line, number);

	if (line == "CONNECT")
		dd->m_type = QStompRequestFrame::RequestConnect;
	else if (line == "SEND")
		dd->m_type = QStompRequestFrame::RequestSend;
	else if (line == "SUBSCRIBE")
		dd->m_type = QStompRequestFrame::RequestSubscribe;
	else if (line == "UNSUBSCRIBE")
		dd->m_type = QStompRequestFrame::RequestUnsubscribe;
	else if (line == "BEGIN")
		dd->m_type = QStompRequestFrame::RequestBegin;
	else if (line == "COMMIT")
		dd->m_type = QStompRequestFrame::RequestCommit;
	else if (line == "ABORT")
		dd->m_type = QStompRequestFrame::RequestAbort;
	else if (line == "ACK")
		dd->m_type = QStompRequestFrame::RequestAck;
	else if (line == "DISCONNECT")
		dd->m_type = QStompRequestFrame::RequestDisconnect;
	else
		return false;

	return true;
}

QByteArray QStompRequestFrame::toByteArray() const
{
	if (!this->isValid())
		return QByteArray("");

	QByteArray ret;
	switch (dd->m_type) {
		case QStompRequestFrame::RequestInvalid:
			return QByteArray("");
		case QStompRequestFrame::RequestConnect:
			ret = "CONNECT\n"; break;
		case QStompRequestFrame::RequestSend:
			ret = "SEND\n"; break;
		case QStompRequestFrame::RequestSubscribe:
			ret = "SUBSCRIBE\n"; break;
		case QStompRequestFrame::RequestUnsubscribe:
			ret = "UNSUBSCRIBE\n"; break;
		case QStompRequestFrame::RequestBegin:
			ret = "BEGIN\n"; break;
		case QStompRequestFrame::RequestCommit:
			ret = "COMMIT\n"; break;
		case QStompRequestFrame::RequestAbort:
			ret = "ABORT\n"; break;
		case QStompRequestFrame::RequestAck:
			ret = "ACK\n"; break;
		case QStompRequestFrame::RequestDisconnect:
			ret = "DISCONNECT\n"; break;
	}
	return ret + QStompFrame::toByteArray();
}

bool QStompRequestFrame::hasDestination() const
{
	return this->headerHasKey("destination");
}

QByteArray QStompRequestFrame::destination() const
{
	return this->headerValue("destination");
}

void QStompRequestFrame::setDestination(const QByteArray &value)
{
	this->setHeaderValue("destination", value);
}

bool QStompRequestFrame::hasTransactionId() const
{
	return this->headerHasKey("transaction");
}

QByteArray QStompRequestFrame::transactionId() const
{
	return this->headerValue("transaction");
}

void QStompRequestFrame::setTransactionId(const QByteArray &value)
{
	this->setHeaderValue("transaction", value);
}

bool QStompRequestFrame::hasMessageId() const
{
	return this->headerHasKey("message-id");
}

QByteArray QStompRequestFrame::messageId() const
{
	return this->headerValue("message-id");
}

void QStompRequestFrame::setMessageId(const QByteArray &value)
{
	this->setHeaderValue("message-id", value);
}

bool QStompRequestFrame::hasReceiptId() const
{
	return this->headerHasKey("receipt");
}

QByteArray QStompRequestFrame::receiptId() const
{
	return this->headerValue("receipt");
}

void QStompRequestFrame::setReceiptId(const QByteArray &value)
{
	this->setHeaderValue("receipt", value);
}

bool QStompRequestFrame::hasAckType() const
{
	return this->headerHasKey("ack");
}

QStompRequestFrame::AckType QStompRequestFrame::ackType() const
{
	if (this->headerValue("ack") == "client")
		return QStompRequestFrame::AckClient;
	return QStompRequestFrame::AckAuto;
}

void QStompRequestFrame::setAckType(QStompRequestFrame::AckType type)
{
	this->setHeaderValue("ack", (type == QStompRequestFrame::AckClient ? "client" : "auto"));
}

bool QStompRequestFrame::hasSubscriptionId() const
{
	return this->headerHasKey("id");
}

QByteArray QStompRequestFrame::subscriptionId() const
{
	return this->headerValue("id");
}

void QStompRequestFrame::setSubscriptionId(const QByteArray &value)
{
	this->setHeaderValue("id", value);
}


QStompClient::QStompClient(QObject *parent) : QObject(parent), d(new QStompClientPrivate)
{
	d->m_socket = NULL;
	d->m_textCodec = QTextCodec::codecForName("utf-8");
}

QStompClient::~QStompClient()
{
	delete this->d;
}

void QStompClient::connectToHost(const QString &hostname, quint16 port)
{
	if (d->m_socket != NULL && d->m_socket->parent() == this)
		delete d->m_socket;
	d->m_socket = new QTcpSocket(this);
	connect(d->m_socket, SIGNAL(connected()), this, SIGNAL(socketConnected()));
	connect(d->m_socket, SIGNAL(disconnected()), this, SIGNAL(socketDisconnected()));
	connect(d->m_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SIGNAL(socketStateChanged(QAbstractSocket::SocketState)));
	connect(d->m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SIGNAL(socketError(QAbstractSocket::SocketError)));
	connect(d->m_socket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));
	d->m_socket->connectToHost(hostname, port);
}

void QStompClient::setSocket(QTcpSocket *socket)
{
	if (d->m_socket != NULL && d->m_socket->parent() == this)
		delete d->m_socket;
	d->m_socket = socket;
	connect(d->m_socket, SIGNAL(connected()), this, SLOT(on_socketConnected()));
	connect(d->m_socket, SIGNAL(disconnected()), this, SLOT(on_socketDisconnected()));
	connect(d->m_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(on_socketStateChanged(QAbstractSocket::SocketState)));
	connect(d->m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(on_socketError(QAbstractSocket::SocketError)));
	connect(d->m_socket, SIGNAL(readyRead()), this, SLOT(on_socketReadyRead()));
}

QTcpSocket * QStompClient::socket()
{
	return d->m_socket;
}

void QStompClient::sendFrame(const QStompRequestFrame &frame)
{
	if (d->m_socket == NULL || d->m_socket->state() != QAbstractSocket::ConnectedState)
		return;
	QByteArray serialized = frame.toByteArray();
	serialized.append('\0');
	serialized.append('\n');
	d->m_socket->write(serialized);
}

void QStompClient::login(const QByteArray &user, const QByteArray &password)
{
	QStompRequestFrame frame(QStompRequestFrame::RequestConnect);
	frame.setHeaderValue("login", user);
	frame.setHeaderValue("passcode", password);
	this->sendFrame(frame);
}

void QStompClient::logout()
{
	this->sendFrame(QStompRequestFrame(QStompRequestFrame::RequestDisconnect));
}

void QStompClient::send(const QByteArray &destination, const QString &body, const QByteArray &transactionId, const QStompHeaderList &headers)
{
	QStompRequestFrame frame(QStompRequestFrame::RequestSend);
	frame.setHeaderValues(headers);
	frame.setContentEncoding(d->m_textCodec);
	frame.setDestination(destination);
	frame.setBody(body);
	if (!transactionId.isNull())
		frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::subscribe(const QByteArray &destination, bool autoAck, const QStompHeaderList &headers)
{
	QStompRequestFrame frame(QStompRequestFrame::RequestSubscribe);
	frame.setHeaderValues(headers);
	frame.setDestination(destination);
	if (autoAck)
		frame.setAckType(QStompRequestFrame::AckAuto);
	else
		frame.setAckType(QStompRequestFrame::AckClient);
	this->sendFrame(frame);
}

void QStompClient::unsubscribe(const QByteArray &destination, const QStompHeaderList &headers)
{
	QStompRequestFrame frame(QStompRequestFrame::RequestUnsubscribe);
	frame.setHeaderValues(headers);
	frame.setDestination(destination);
	this->sendFrame(frame);
}

void QStompClient::commit(const QByteArray &transactionId, const QStompHeaderList &headers)
{
	QStompRequestFrame frame(QStompRequestFrame::RequestCommit);
	frame.setHeaderValues(headers);
	frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::begin(const QByteArray &transactionId, const QStompHeaderList &headers)
{
	QStompRequestFrame frame(QStompRequestFrame::RequestBegin);
	frame.setHeaderValues(headers);
	frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::abort(const QByteArray &transactionId, const QStompHeaderList &headers)
{
	QStompRequestFrame frame(QStompRequestFrame::RequestAbort);
	frame.setHeaderValues(headers);
	frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::ack(const QByteArray &messageId, const QByteArray &transactionId, const QStompHeaderList &headers)
{
	QStompRequestFrame frame(QStompRequestFrame::RequestAck);
	frame.setHeaderValues(headers);
	frame.setMessageId(messageId);
	if (!transactionId.isNull())
		frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

int QStompClient::framesAvailable() const
{
	return d->m_framebuffer.size();
}

QStompResponseFrame QStompClient::fetchFrame()
{
	if (d->m_framebuffer.size() > 0)
		return d->m_framebuffer.takeFirst();
	else
		return QStompResponseFrame();
}

QList<QStompResponseFrame> QStompClient::fetchAllFrames()
{
	QList<QStompResponseFrame> frames = d->m_framebuffer;
	d->m_framebuffer.clear();
	return frames;
}

QAbstractSocket::SocketState QStompClient::socketState() const
{
	if (d->m_socket == NULL)
		return QAbstractSocket::UnconnectedState;
	return d->m_socket->state();
}

QAbstractSocket::SocketError QStompClient::socketError() const
{
	if (d->m_socket == NULL)
		return QAbstractSocket::UnknownSocketError;
	return d->m_socket->error();
}

QString QStompClient::socketErrorString() const
{
	if (d->m_socket == NULL)
		return QLatin1String("No socket");
	return d->m_socket->errorString();
}

QByteArray QStompClient::contentEncoding()
{
	return d->m_textCodec->name();
}

void QStompClient::setContentEncoding(const QByteArray & name)
{
	d->m_textCodec = QTextCodec::codecForName(name);
}

void QStompClient::setContentEncoding(const QTextCodec * codec)
{
	d->m_textCodec = codec;
}

void QStompClient::disconnectFromHost()
{
	if (d->m_socket != NULL)
		d->m_socket->disconnectFromHost();
}

void QStompClient::socketReadyRead()
{
	QByteArray data = d->m_socket->readAll();
	d->m_buffer.append(data);

	quint32 length;
	bool gotOne = false;
	while ((length = d->findMessageBytes())) {
		QStompResponseFrame frame(d->m_buffer.left(length));
		if (frame.isValid()) {
			d->m_framebuffer.append(frame);
			gotOne = true;
		}
		else
			qDebug("QStomp: Invalid frame received!");
		d->m_buffer.remove(0, length);
	}
	if (gotOne)
		emit frameReceived();
}


quint32 QStompClientPrivate::findMessageBytes()
{
	// Buffer sanity check
	forever {
		if (this->m_buffer.isEmpty())
			return 0;
		int nl = this->m_buffer.indexOf('\n');
		if (nl == -1)
			break;
		QByteArray cmd = this->m_buffer.left(nl);
		if (VALID_COMMANDS.contains(cmd))
			break;
		else {
			qDebug("QStomp: Framebuffer corrupted, repairing...");
			int syncPos = this->m_buffer.indexOf(QByteArray("\0\n", 2));
			if (syncPos != -1)
				this->m_buffer.remove(0, syncPos+2);
			else {
				syncPos = this->m_buffer.indexOf(QByteArray("\0", 1));
				if (syncPos != -1)
					this->m_buffer.remove(0, syncPos+1);
				else
					this->m_buffer.clear();
			}
		}
	}

	// Look for content-length
	int headerEnd = this->m_buffer.indexOf("\n\n");
	int clPos = this->m_buffer.indexOf("\ncontent-length");
	if (clPos != -1 && headerEnd != -1 && clPos < headerEnd) {
		int colon = this->m_buffer.indexOf(':', clPos);
		int nl = this->m_buffer.indexOf('\n', clPos);
		if (colon != -1 && nl != -1 && nl > colon) {
			bool ok = false;
			quint32 cl = this->m_buffer.mid(colon, nl-colon).toUInt(&ok) + headerEnd+2;
			if (ok) {
				if ((quint32)this->m_buffer.size() >= cl)
					return cl;
				else
					return 0;
			}
		}
	}

	// No content-length, look for \0\n
	int end = this->m_buffer.indexOf(QByteArray("\0\n", 2));
	if (end == -1) {
		// look for \0
		end = this->m_buffer.indexOf(QByteArray("\0", 1));
		if (end == -1)
			return 0;
		else
			return (quint32) end+1;
	}
	else
		return (quint32) end+2;
}
