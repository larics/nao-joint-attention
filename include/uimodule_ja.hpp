#ifndef INTERFACE_JA_H
#define INTERFACE_JA_H

#include <boost/shared_ptr.hpp>
#include <alcommon/almodule.h>
#include <string>

#include <alproxies/almemoryproxy.h>
#include <alproxies/alaudioplayerproxy.h>
#include <alproxies/albehaviormanagerproxy.h>
#include <althread/almutex.h>

namespace AL
{
  class ALBroker;
}

/**
  * Interface class used to reproduce sound to call the child
  * Reacts to events generated by the instance of the Logger clas
  */
class JointAttentionInterface : public AL::ALModule
{
  public:

    /**
      * Standard default constructor, called by the NAOqi of the robot
      */
    JointAttentionInterface(boost::shared_ptr<AL::ALBroker> broker, const std::string& name);

    /**
      * Default destructor
      */
    virtual ~JointAttentionInterface();

    /**
      * Method overloading overloading ALModule::init().
      * This is called right after the module has been loaded by NAOqi
      */
    virtual void init();

    /**
      * Method used to start/enable the task
      */
    void startTask(const std::string& todo);

    /**
      * This method will be called when FrontTactilTouched event is raised
      * When called, it raises StartSession event
      */
    void onTactilTouched();

    /**
      * This method will be called when CallChild event is raised
      * Event is raised by the scheduler thread of Logger module
      * Method uses proxy to ALAudioDevice to reproduce the call by name
      */
    void callChild(const std::string &key, const AL::ALValue &value, const AL::ALValue &msg);

    /**
      * This method will be called when EndSession event is raised by the Logger module
      * Resets the Interface module, subscribing back to FrontTactilTouched, enables new session
      */
    void endSession();

    /**
      * Method used to enable starting of the task
      */
    void enableTask(const std::string &IP, const int &port);

  private:
    /**
      * Object implementation
      */
    struct Impl;
    /**
      * Pointer to class implementation
      */
    boost::shared_ptr<Impl> impl;
};

#endif
