// Copyright 2021 PAL Robotics S.L.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the PAL Robotics S.L. nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.


#ifndef PAL_GAZEBO_PLUGINS__PAL_GAZEBO_COLLISIONS_HPP_
#define PAL_GAZEBO_PLUGINS__PAL_GAZEBO_COLLISIONS_HPP_

#include "rclcpp/rclcpp.hpp"

// Gazebo
#include "gazebo/gazebo.hh"
#include "gazebo/common/common.hh"
#include "gazebo/common/Event.hh"
#include "gazebo/physics/physics.hh"
#include "gazebo_msgs/msg/contacts_state.hpp"
#include "gazebo_msgs/msg/contact_state.hpp"

namespace gazebo
{

class GazeboCollisions : public WorldPlugin
{
public:
  virtual void Load(physics::WorldPtr _world, sdf::ElementPtr _sdf);
  virtual void OnUpdate(const common::UpdateInfo &);

private:
  physics::WorldPtr world_;
  physics::Contact * contact_;

  gazebo::event::ConnectionPtr connection_;
  double update_rate_;
  rclcpp::Time last_time_published_;

  rclcpp::Node::SharedPtr node_;
  rclcpp::Publisher<gazebo_msgs::msg::ContactsState>::SharedPtr collisions_pub_;
};

}  // namespace gazebo

#endif  // PAL_GAZEBO_PLUGINS__PAL_GAZEBO_COLLISIONS_HPP_
