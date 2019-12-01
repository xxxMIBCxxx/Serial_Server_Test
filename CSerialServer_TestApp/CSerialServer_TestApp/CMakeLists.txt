cmake_minimum_required(VERSION 3.8)

add_executable(CSerialServer_TestApp
	main.cpp
	CEvent.cpp
	CEventEx.cpp
	CMutex.cpp
	CSerialServer.cpp
	CServerConnectMonitoringThread.cpp
	CServerResponseThread.cpp
	CServerTcpRecvThread.cpp
	CTcpRecvThread.cpp
	CTcpSendThread.cpp
	CThread.cpp
)

set(CMAKE_CXX_FLAGS "-g -O0 -lpthread")

set(INCLUDE_DIRECTORIES
	/usr/include
	/usr/include/arm-linux-gnueabihf
	${PROJECT_SOCE_DIR}
)

