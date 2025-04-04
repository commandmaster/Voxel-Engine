#pragma once

class Application
{
public:
	Application() = default;
	virtual ~Application() = default;
	
	virtual void init();
	virtual void run();

	Application(const Application&) = delete;
	Application& operator= (const Application&) = delete;
};

// The client must create the application
Application* CreateApplication();

