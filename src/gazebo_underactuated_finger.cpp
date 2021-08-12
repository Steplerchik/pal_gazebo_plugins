/*
 *  gazebo_pal_hey5.cpp
 *  Copyright (c) 2015 PAL Robotics sl. All Rights Reserved
 *  Created on: 6/3/2015
 *      Author: luca
 * \brief A plugin for gazebo for controlling the pal hey5 underactuated hand in simulation
 * \author  Luca Marchionni (luca.marchionni@pal-robotics.com)
 */


///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015, PAL Robotics S.L.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// * Neither the name of PAL Robotics S.L. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//////////////////////////////////////////////////////////////////////////////


#include <pal_gazebo_plugins/gazebo_underactuated_finger.hpp>

#include <ignition/math.hh>
#include <sdf/sdf.hh>

#include <rclcpp/rclcpp.hpp>

#include <assert.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>

namespace gazebo {

GazeboPalHey5::GazeboPalHey5() {}

// Destructor
GazeboPalHey5::~GazeboPalHey5() {
}

// Load the controller
void GazeboPalHey5::Load(physics::ModelPtr _parent, sdf::ElementPtr _sdf) {
  this->parent = _parent;
  this->world = _parent->GetWorld();

  this->actuated_joint_name_ = "actuated_finger_joint";
  if (!_sdf->HasElement("actuatedJoint"))
  {
    char error[200];
    snprintf(error, sizeof(error),
             "GazeboPalHey5 Plugin (ns = %s) missing actuatedJoint in urdf plugin description",
             this->robot_namespace_.c_str());
    gzthrow(error);
  } else {
    this->actuated_joint_name_ = _sdf->GetElement("actuatedJoint")->Get<std::string>();
  }

  this->robot_namespace_ = "";
  if (_sdf->HasElement("robotNamespace"))
  {
    this->robot_namespace_ =
      _sdf->GetElement("robotNamespace")->Get<std::string>() + "/";

    ros_node_ = std::make_shared<rclcpp::Node>(this->actuated_joint_name_,
                                               this->robot_namespace_);
  } else {
    ros_node_ = std::make_shared<rclcpp::Node>(this->actuated_joint_name_);

    RCLCPP_INFO(ros_node_->get_logger(),
                "GazeboPalHey5 Plugin missing <robotNamespace>, defaults to \"%s\"",
                this->robot_namespace_.c_str());
  }

  if(!_sdf->HasElement("virtualJoint"))
  {
    char error[200];
    snprintf(error, sizeof(error),
             "GazeboPalHey5 Plugin (ns = %s) missing virtualJoint in urdf plugin description",
             this->robot_namespace_.c_str());
    gzthrow(error);
  }
  for(sdf::ElementPtr virtualJointPtr = _sdf->GetElement(std::string("virtualJoint"));
      virtualJointPtr;
      virtualJointPtr = _sdf->GetElement(std::string("virtualJoint")))
  {
    if(virtualJointPtr->HasElement("name")){
      std::string virtualJointName = virtualJointPtr->GetElement("name")->Get<std::string>();
      virtual_joint_names_.push_back(virtualJointName);
    }

    if(virtualJointPtr->HasElement(std::string("scale_factor"))){
      double scaleFactor = virtualJointPtr->GetElement(std::string("scale_factor"))->Get<double>();
      scale_factors_.push_back(scaleFactor);
    }

    if(virtualJointPtr->HasElement(std::string("pid_gains")))
    {
      sdf::ElementPtr pidGainsPtr = virtualJointPtr->GetElement(std::string("pid_gains"));
      std::map<std::string, double> pidGains;

      for (std::string const& gain : {"p", "i", "d", "i_max", "i_min"})
      {
        if (pidGainsPtr->HasElement(std::string(gain)))
        {
          pidGains.emplace(gain, pidGainsPtr->GetElement(std::string(gain))->Get<double>());
        }
      }

      pid_gains_.emplace_back(pidGains);
    } else {
      pid_gains_.emplace_back();
    }

    // Removing element to parse the next one, no better solution found
    virtualJointPtr->RemoveFromParent();
    if(!_sdf->HasElement("virtualJoint"))
      break;
  }

  gazebo::physics::JointPtr joint = this->parent->GetJoint(actuated_joint_name_);
  if(!joint)
  {
    char error[200];
    snprintf(error, sizeof(error),
             "GazeboPalHey5 Plugin (ns = %s) couldn't get actuated finger hinge joint named \"%s\"",
             this->robot_namespace_.c_str(), this->actuated_joint_name_.c_str());
    gzthrow(error);
  }
  actuated_joint_ = joint;
  actuator_angle_ = actuated_joint_->Position(0u);

  for(unsigned int i = 0 ; i < virtual_joint_names_.size(); ++i)
  {
    gazebo::physics::JointPtr joint_ptr = this->parent->GetJoint(virtual_joint_names_.at(i));

    if (!joint_ptr) {
      char error[200];
      snprintf(
            error, sizeof(error),
            "GazeboPalHey5 Plugin (ns = %s) couldn't get actuated finger hinge joint named \"%s\"",
            this->robot_namespace_.c_str(), this->virtual_joint_names_.at(i).c_str());
      gzthrow(error);
    }

    virtual_joints_.push_back(joint_ptr);

    std::shared_ptr<rclcpp::Node> node_ptr(new rclcpp::Node(this->virtual_joint_names_.at(i),
                                                            this->robot_namespace_ +
                                                            "virtual_joints/" +
                                                            this->virtual_joint_names_.at(i)));
    PidROSPtr pid(new control_toolbox::PidROS(node_ptr, node_ptr->get_name()));

    try
    {
      double p_param = pid_gains_.at(i).at("p");
      double i_param = pid_gains_.at(i).at("i");
      double d_param = pid_gains_.at(i).at("d");
      double i_max_param = pid_gains_.at(i).at("i_max");
      double i_min_param = pid_gains_.at(i).at("i_min");

      pid->initPid(p_param, i_param, d_param, i_max_param, i_min_param, /*antiwindup*/ false);

      pids_.push_back(pid);
    }
    catch (std::out_of_range&)
    {
      RCLCPP_ERROR(node_ptr->get_logger(),
                   "Did not find a complete pid configutation in the urdf for \"%s\"",
                   this->virtual_joint_names_.at(i));
      pids_.push_back(nullptr);
    }
  }
  // listen to the update event (broadcast every simulation iteration)
  this->update_connection_ =
    event::Events::ConnectWorldUpdateBegin(
        boost::bind(&GazeboPalHey5::UpdateChild, this));

  std::string init_str = "Initialized GazeboPalHey5 finger with actuator: " +
      this->actuated_joint_name_;
  init_str += " and virtual joints: ";
  for(unsigned int i = 0 ; i < virtual_joint_names_.size(); ++i)
  {
   init_str += this->virtual_joint_names_.at(i);
   init_str += " ";
  }
  RCLCPP_INFO(ros_node_->get_logger(), init_str);
}

// Update the controller
void GazeboPalHey5::UpdateChild() {
  ignition::math::Angle new_actuator_angle = actuated_joint_->Position(0u);

  // Filter for noisy measure of actuated angle
  double ang_err_rad = (actuator_angle_-new_actuator_angle).Radian();
  double eps_angle_rad = 0.02;
  if( fabs( ang_err_rad ) > eps_angle_rad)
    actuator_angle_ = new_actuator_angle;

  for(unsigned int i = 0; i < virtual_joints_.size(); ++i)
  {
    ignition::math::Angle new_angle  = actuator_angle_/scale_factors_.at(i);

    if(new_angle >  virtual_joints_.at(i)->UpperLimit(0u))
      new_angle = virtual_joints_.at(i)->UpperLimit(0u);
    if(new_angle < virtual_joints_.at(i)->LowerLimit(0u))
      new_angle = virtual_joints_.at(i)->LowerLimit(0u);

    if(pids_.at(i))
    {
      double pos = virtual_joints_.at(i)->Position(0);
      double error = new_angle.Radian() - pos;
      const double effort = pids_.at(i)->computeCommand(error,
                                                        rclcpp::Duration::from_seconds(0.001));
      virtual_joints_.at(i)->SetForce(0, effort);
    } else {
      virtual_joints_.at(i)->SetPosition(0u, new_angle.Radian());
    }
  }
}

GZ_REGISTER_MODEL_PLUGIN(GazeboPalHey5)

}  // namespace gazebo


