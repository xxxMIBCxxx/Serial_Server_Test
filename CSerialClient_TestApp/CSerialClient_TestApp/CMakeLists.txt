cmake_minimum_required(VERSION 3.8)

add_executable(CSerialClient_TestApp
	main.cpp
	CEvent.cpp
	CEventEx.cpp
	CMutex.cpp
	CClientResponseThread.cpp
	CClientTcpRecvThread.cpp
	CSerialClient.cpp
	CTcpRecvThread.cpp
	CThread.cpp
)

set(CMAKE_CXX_FLAGS "-Wall -g -O0 -lpthread")

set(INCLUDE_DIRECTORIES
	/usr/include
	/usr/include/arm-linux-gnueabihf
	${PROJECT_SOCE_DIR}
)

