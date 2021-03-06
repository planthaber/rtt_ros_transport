/***************************************************************************
  tag: Ruben Smits  Tue Nov 16 09:18:49 CET 2010  ros_publish_activity.hpp

                        ros_publish_activity.hpp -  description
                           -------------------
    begin                : Tue November 16 2010
    copyright            : (C) 2010 Ruben Smits
    email                : first.last@mech.kuleuven.be

 ***************************************************************************
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Lesser General Public            *
 *   License as published by the Free Software Foundation; either          *
 *   version 2.1 of the License, or (at your option) any later version.    *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 59 Temple Place,                                    *
 *   Suite 330, Boston, MA  02111-1307  USA                                *
 *                                                                         *
 ***************************************************************************/


#ifndef _ROS_MSG_ACTIVITY_HPP_
#define _ROS_MSG_ACTIVITY_HPP_

#include <rtt/Activity.hpp>
#include <rtt/os/Mutex.hpp>
#include <rtt/os/MutexLock.hpp>
#include <boost/shared_ptr.hpp>
#include <rtt/Logger.hpp>

#include <ros/ros.h>

namespace RTT{
namespace ros{
    /**
     * The interface a channel element must implement in
     * order to publish data to a ROS topic.
     */
  struct RosPublisher
  {
  public:
      /**
       * Publish all data in the channel to a ROS topic.
       */
      virtual void publish()=0;
  };
  

    /**
     * A process wide thread that handles all publishing of
     * ROS topics of the current process.
     * There is no strong reason why only one publisher should
     * exist, in later implementations, one publisher thread per
     * channel may exist as well. See the usage recommendations
     * for Instance() 
     */
  class RosPublishActivity : public RTT::Activity {
  public:
      typedef boost::shared_ptr<RosPublishActivity> shared_ptr;
  private:
      typedef boost::weak_ptr<RosPublishActivity> weak_ptr;
      //! This pointer may not be refcounted since it would prevent cleanup.
      static weak_ptr ros_pub_act;
    
      //! A map keeping track of all publishers in the current
      //! process. It must be guarded by the mutex since 
      //! insertion/removal happens concurrently.
      typedef std::map< RosPublisher*, bool> Publishers;
      Publishers publishers;
      os::Mutex map_lock;

    RosPublishActivity( const std::string& name)
        : Activity(ORO_SCHED_OTHER, RTT::os::LowestPriority, 0.0, 0, name)
    {
      Logger::In in("RosPublishActivity");
      log(Debug)<<"Creating RosPublishActivity"<<endlog();
    }

    void loop(){
        os::MutexLock lock(map_lock);
        for(Publishers::iterator it = publishers.begin(); it != publishers.end(); ++it)
            if (it->second) {
                it->second = false; // protected by the mutex lock !
                it->first->publish();
            }
    }
    
  public:

      /**
       * Returns the single instance of the RosPublisher. This function
       * is not thread-safe when it creates the RosPublisher object.
       * Therefor, it is advised to cache the object which Intance() returns
       * such that, in the unlikely event that two publishers exist,
       * you consistently keep using the same instance, which is fine then.
       */
      static shared_ptr Instance() {
          shared_ptr ret = ros_pub_act.lock();
          if ( !ret ) {
              ret.reset(new RosPublishActivity("RosPublishActivity"));
              ros_pub_act = ret;
              ret->start();
          }
          return ret;
      }
      
      void addPublisher(RosPublisher* pub) {
          os::MutexLock lock(map_lock);
          publishers[pub] = false;
      }

      void removePublisher(RosPublisher* pub) {
          os::MutexLock lock(map_lock);
          publishers.erase(pub);
      }
    
      /**
       * Requests to publish the data of a given channel.
       * Note that multiple calls to requestPublish may 
       * cause only a single call to RosPublisher::publish().
       */
      bool requestPublish(RosPublisher* chan){
          // flag that data is available in a channel:
          {
              os::MutexLock lock(map_lock);
              assert(publishers.find(chan) != publishers.end() );
              publishers.find(chan)->second = true;
          }
          // trigger loop()
          this->trigger();
          return true;
      }
      ~RosPublishActivity() {
          Logger::In in("RosPublishActivity");
          log(Info) << "RosPublishActivity cleans up: no more work."<<endlog();
          stop();
      }
      
  };//class
}}//namespace
#endif

