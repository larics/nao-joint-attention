/**
 * Author: Frano Petric
 * Version: 0.9
 * Date: 23.4.2014.
 */

#include "uimodule_ja.hpp"
#include <iostream>
#include <fstream>
#include <alvalue/alvalue.h>
#include <alcommon/alproxy.h>
#include <alcommon/albroker.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <boost/lambda/lambda.hpp>
#include <qi/log.hpp>
#include <althread/alcriticalsection.h>

struct Interface::Impl {

    /**
      * IP and port of the remote NAOqi which is running on the other robot
      */
    std::string remoteIP;
    int remotePort;

    /**
      * Proxy to local ALMemory
      */
    boost::shared_ptr<AL::ALMemoryProxy> memoryProxy;
    /**
      * Proxy to remote ALMemory (running on the other robot)
      */
    boost::shared_ptr<AL::ALMemoryProxy> memoryProxyRemote;
    /**
      * Proxy to ALBehaviorManager for running pointing behavior
      */
    boost::shared_ptr<AL::ALBehaviorManagerProxy> behaviorProxy;
    /**
      * Proxy to ALAudioPlayer for sound reproduction
      */
    boost::shared_ptr<AL::ALAudioPlayerProxy> playerProxy;

    /**
      * Module object
      */
    Interface &module;

    /**
      * Mutex used to lock callback functions, making them thread safe
      */
    boost::shared_ptr<AL::ALMutex> fCallbackMutex;

    /**
      * Function reading config file containing IP and port of the other robot
      */
    void readConfig(std::string& IP, int& port) {
        // Open config file
        std::ifstream config("/home/nao/naoqi/modules/config/remote.conf");
        // Read IP and port
        config >> IP >> port;
        qiLogWarning("Interface") << "Connecting to " << IP << ":" << port << std::endl;
        // Close
        config.close();
    }

    /**
      * Function used to call and point to other robot
      */
    void call(std::string filename) {
        // Call by name (using unblocking call)
        //playerProxy->post.playFile(filename);
        // Run pointing behavior
        //behaviorProxy->runBehavior("point");
        qiLogWarning("Interface") << "Calling" << std::endl;
    }

    /**
      * Struct constructor, initializes module instance and callback mutex
      */
    Impl(Interface &mod) : module(mod), fCallbackMutex(AL::ALMutex::createALMutex()) {

        // Create proxies
        try {
            memoryProxy = boost::shared_ptr<AL::ALMemoryProxy>(new AL::ALMemoryProxy(mod.getParentBroker()));
            behaviorProxy = boost::shared_ptr<AL::ALBehaviorManagerProxy>(new AL::ALBehaviorManagerProxy(mod.getParentBroker()));
            playerProxy = boost::shared_ptr<AL::ALAudioPlayerProxy>(new AL::ALAudioPlayerProxy(mod.getParentBroker()));
        }
        catch (const AL::ALError& e) {
            qiLogError("Interface") << "Error creating proxies: " << e.toString() << std::endl;
        }
        // Declare events that are generated by this module
        memoryProxy->declareEvent("StartSession");
        memoryProxy->declareEvent("ChildCalled");

        // Subscribe to event FronTactilTouched, which signals the start of the session
        memoryProxy->subscribeToEvent("FrontTactilTouched", "Interface", "onTactilTouched");
    }
};

Interface::Interface(boost::shared_ptr<AL::ALBroker> pBroker, const std::string& pName) :  AL::ALModule(pBroker, pName) {

    setModuleDescription("Interface module, reacting to events generated by the Logger module, calling child by either name or by using special phrases");

    functionName("onTactilTouched", getName(), "FrontTactilTouched callback, starts the session");
    BIND_METHOD(Interface::onTactilTouched);

    functionName("callChild", getName(), "CallChild callback, plays the sound");
    addParam("key", "Memory key storing data related to the event");
    addParam("value", "Value with which event is raised");
    addParam("msg", "Message provided by the module which generated the event");
    BIND_METHOD(Interface::callChild);

    functionName("endSession", getName(), "EndSession callback, resets the Interface");
    BIND_METHOD(Interface::endSession);

    functionName("enableTask", getName(), "Method to enable the task by subscribing to the FronTactilTouched event");
    BIND_METHOD(Interface::enableTask);

//    functionName("setRemoteConnection", getName(), "Method to enable the task by subscribing to the FronTactilTouched event");
//    BIND_METHOD(Interface::setRemoteConnection);
}

Interface::~Interface() {
    // Cleanup code
}

void Interface::init() {
   // This method overrides ALModule::init
    try {
        // Create object
        impl = boost::shared_ptr<Impl>(new Impl(*this));
        // Initialize ALModule
        AL::ALModule::init();
    }
    catch (const AL::ALError& e) {
        qiLogError("Interface") << e.what() << std::endl;
    }
    qiLogVerbose("Interface") << "Interface initialized" << std::endl;
}

void Interface::onTactilTouched() {
    // Callback is thread safe as long as ALCriticalSection object exists
    AL::ALCriticalSection section(impl->fCallbackMutex);
    // Unsubscribe from the event
    impl->memoryProxy->unsubscribeToEvent("FrontTactilTouched", "Interface");
    // Open connection to the other robot
    impl->readConfig(impl->remoteIP, impl->remotePort);
    try {
       impl->memoryProxyRemote = boost::shared_ptr<AL::ALMemoryProxy>(new AL::ALMemoryProxy(impl->remoteIP, impl->remotePort));
    }
    catch (const AL::ALError& e) {
        qiLogError("Interface") << "Error connecting to the other robot: " << e.toString() << std::endl;
    }
    // Subscribe to events which can be triggered during the session
    // These events are generated by the other robot
    try {
        impl->memoryProxyRemote->subscribeToEvent("CallChild", "Interface", "callChild");
        impl->memoryProxyRemote->subscribeToEvent("EndSession", "Interface", "endSession");
    }
    catch (const AL::ALError& e) {
        qiLogError("Interface") << "Error subscribing to events: " << e.toString() << std::endl;
    }
    // Raise event that the session should start
    impl->memoryProxy->raiseEvent("StartSession", AL::ALValue(1));
}

void Interface::callChild(const std::string &key, const AL::ALValue &value, const AL::ALValue &msg) {
    // Thread safety
    AL::ALCriticalSection section(impl->fCallbackMutex);
    // Unsubscribing
    impl->memoryProxyRemote->unsubscribeToEvent("CallChild", "Interface");

    // Reproduce the sound using ALAudioDevice proxy
    if( (int)value == 1 ) {
        // If event is raised with value 1, call child by name with pointing action towards other robot
        qiLogVerbose("Interface") << "Calling with name\n";
        // TODO: enable calling by uncommenting the following line
        impl->call("home/nao/naoqi/sounds/name.wav");
    }
    else if ( (int)value == 2 ) {
        // Event is raised with value 2, use special phrase
        qiLogVerbose("Interface") << "Calling with special phrase\n";
        // TODO: enable the player by uncommenting following line
        //impl->playerProxy->playFile("home/nao/naoqi/sounds/phrase.wav");
    }
    // Notify the Logger module that child was called
    impl->memoryProxy->raiseEvent("ChildCalled", value);

    // Subscribe to the CallChild event again
    impl->memoryProxyRemote->subscribeToEvent("CallChild", "Interface", "callChild");
}

void Interface::endSession() {
    // Thread safety
    AL::ALCriticalSection section(impl->fCallbackMutex);
    // Unsubscribe
    impl->memoryProxyRemote->unsubscribeToEvent("EndSession", "Interface");

    // Reset subscriptions
    try {
        impl->memoryProxyRemote->unsubscribeToEvent("CallChild", "Interface");
        impl->memoryProxy->subscribeToEvent("FrontTactilTouched", "Interface", "onTactilTouched");
    }
    catch (const AL::ALError& e) {
        qiLogError("Interface") << "Error managing events while reseting" << e.toString() << std::endl;
    }
}

//void Interface::setRemoteConnection(std::string& IP, int& port) {
//    impl->remoteIP = IP;
//    impl->remotePort = port;
//    // Open proxy to ALMemory of the other robot
//    try{
//        impl->memoryProxyRemote = boost::shared_ptr<AL::ALMemoryProxy>(new AL::ALMemoryProxy(impl->remoteIP, impl->remotePort));
//    }
//    catch (const AL::ALError& e) {
//        qiLogError("Interface") << "Error connecting to the other robot: " << e.toString() << std::endl;
//    }
//}

void Interface::enableTask(const std::string &IP, const int &port) {
    impl->remoteIP = IP;
    impl->remotePort = port;
    // Open proxy to ALMemory of the other robot
    try{
        impl->memoryProxyRemote = boost::shared_ptr<AL::ALMemoryProxy>(new AL::ALMemoryProxy(impl->remoteIP, impl->remotePort));
    }
    catch (const AL::ALError& e) {
        qiLogError("Interface") << "Error connecting to the other robot: " << e.toString() << std::endl;
    }
    // enable starting of the task by subscribing to FrontTactilTouched
    impl->memoryProxy->subscribeToEvent("FrontTactilTouched", "Interface", "onTactilTouched");
}
