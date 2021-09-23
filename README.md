# XMPP
Стек: С++11.
Простая реализация "Jabber" ботов по протоколу XMPP(сигнал-слот).
В основном для реализации просты ботов.

Дополнительные модули(библиотеки): Boost, Cryptopp, Pugixml.

Потдерживается(рекомендуется): SSL подключение(не TLS), keep-alive(потдержка подключения активным), debug-mode

Авторизация:
    AUTO(Клиент выберет самостоятельно из доступных)
    DigestMD5,
    SCRAM_SHA1
    Plain

Реализованные возможности:
  Получение ростера,
  Получение сообщения от контакта,
  Получение уведомление о прочтении контактом сообщения,
  Отправка сообщения контакту,
  Отправка уведомление о прочтении сообщения контакту.
  Чтение конфига из XML файла
  
Примеры: 
  Echo: Получение сообщений от контакта отправка этого же сообщения обратно
  Jabber: Полная визуализация всех возможностей.
  
Сборка: g++ example/{нужный проект}.cpp XMPP/xmpp.cpp XMPP/networkclient.cpp XMPP/authorization.cpp XMPP/package.cpp pugixml/pugixml.cpp -lcryptopp -lcrypto -lssl -lpthread -lboost_thread -lboost_system -std=c++11 -o {имя приложения} 

