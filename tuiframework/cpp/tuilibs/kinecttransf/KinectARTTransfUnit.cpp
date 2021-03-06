/*
    Copyright (C) 2010, 2011, 2012 The Fraunhofer Institute for Production Systems and
    Design Technology IPK. All rights reserved.

    This file is part of the TUIFramework library.
    It includes a software framework which contains common code
    providing generic functionality for developing applications
    with a tangible user interface (TUI).
    
    The TUIFramework library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    The TUIFramework is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with the TUIFramework.  If not, see <http://www.gnu.org/licenses/>.
*/



#include "KinectARTTransfUnit.h"
#include <tuiframework/logging/Logger.h>
#include <tuitypes/common/CommonTypeReg.h>
#include <sstream>

using namespace std;

namespace tuiframework {


KinectARTTransfUnit::KinectARTTransfUnit() :
        eventSink(0) {

    /// some default value for the local/desktop case -> kinect on stativ values
    rot_trans[0]= -0.999751;
    rot_trans[1]= 0.020905;
    rot_trans[2]= -0.007768;
    rot_trans[3]= 0.0;

    rot_trans[4]= 0.022023;
    rot_trans[5]= 0.980283;
    rot_trans[6]= -0.196365;
    rot_trans[7]= 0.0;

    rot_trans[8]= 0.00351;
    rot_trans[9]= -0.196487;
    rot_trans[10]= -0.9805;
    rot_trans[11]= 0.0;

    rot_trans[12]= 0.0;
    rot_trans[13]= 0.0;
    rot_trans[14]= -1.5;
    rot_trans[15]= 1.0;

    /// todo: optimize that, translation of the origin of
    /// the kinect coordinate system to the origin of the art coordinate system
    translX = 0.0;
    translY = +0.30;
    translZ = -0.02;

    filterTreshhold = 1.0f;

    m_dev = 0;
    clicked = false;
    //0 bezier, 1 tong ,2 pen
}


KinectARTTransfUnit::~KinectARTTransfUnit() {
}


void KinectARTTransfUnit::setParameterGroup(const ParameterGroup & parameterGroup) {
}


void KinectARTTransfUnit::setEventSink(IEventSink * eventSink) {
    this->eventSink = eventSink;
}

void  KinectARTTransfUnit::setEventSinkMap(std::map<std::string, IEventSink *> * EventSinkMap){
    this->registeredEventSinkMap = EventSinkMap;
}

void KinectARTTransfUnit::push(IEvent * event) {

    //KinectEvent -> skeleton joint
    if(event->getEventTypeID() == 13){
        KinectEvent * e = static_cast<KinectEvent *>(event);
        switch (e->getPayload().getJointId()){
             case KinectJoint::SKEL_LEFT_HAND:
                LEFT_HAND_Changed(e);
                break;
            case KinectJoint::SKEL_RIGHT_HAND:
                RIGHT_HAND_Changed(e);
                break;
            case KinectJoint::SKEL_LEFT_ELBOW:
                LEFT_ELBOW_Changed(e);
                break;
            case KinectJoint::SKEL_RIGHT_ELBOW:
                RIGHT_ELBOW_Changed(e);
                break;
             default:
                break;

        }
    }
    //GestureEvent
    else if(event->getEventTypeID() == 14){
        this->GESTURE(static_cast<GestureEvent *>(event));
    }
    //Matrix4ChangedEvent -> Kinect ART pose
    else if(event->getEventTypeID() == 10){
        this->kinectPoseChanged(static_cast<Matrix4ChangedEvent *>(event));
    }
    else{
        std::cout << "should not happen" << std::endl;
    }
}



void KinectARTTransfUnit::kinectPoseChanged(const Matrix4ChangedEvent * event) {
    rot_trans = event->getPayload();
    rot_trans[12] *= 0.001;
    rot_trans[13] *= 0.001;
    rot_trans[14] *= 0.001;

}

void KinectARTTransfUnit::HEAD_Changed( const KinectEvent * event ) {

}

void KinectARTTransfUnit::TORSO_Changed( const KinectEvent * event ) {
    //std::cout << "torso changed" << std::endl;
}

void KinectARTTransfUnit::NECK_Changed(const KinectEvent * event){
    //std::cout << "neck changed" << std::endl;
}

void KinectARTTransfUnit::LEFT_HAND_Changed( const KinectEvent * event) {
    KinectJoint receivedJointData = event->getPayload();

    // get the joint position in kinect coordinates
	float xl = receivedJointData.getPosition().getX() + translX;
	float yl = receivedJointData.getPosition().getY() + translY;
	float zl = receivedJointData.getPosition().getZ() + translZ;

    //roation by 180 degrees , z-axis of
	xl *= -1.0;
	zl *= -1.0;

    //rotation and translation: tracker to cave
	float xl_ = rot_trans[0]*xl + rot_trans[4]*yl + rot_trans[8]*zl + rot_trans[12];
	float yl_ = rot_trans[1]*xl + rot_trans[5]*yl + rot_trans[9]*zl + rot_trans[13];
	float zl_ = rot_trans[2]*xl + rot_trans[6]*yl + rot_trans[10]*zl + rot_trans[14];

    m_lh = Vector3d((1-filterTreshhold)*m_lh.getX() + filterTreshhold*xl_, (1-filterTreshhold)*m_lh.getY() + filterTreshhold*yl_, (1-filterTreshhold)*m_lh.getZ() + filterTreshhold*zl_);

    Vector3d res(m_lh.getX(), m_lh.getY(), m_lh.getZ());
    res.subtract(m_le);

    if(res.getX()*res.getX() + res.getY()*res.getY() + res.getZ()*res.getZ() > 0.0)
        res.normalize();
    else
        return;

    Matrix4Data mat;
    Matrix4ChangedEvent * mat_event = new Matrix4ChangedEvent();

    mat.setRow(0, 0, 0, 0, 0);
    mat.setRow(1, 0, 0, 0, 0);
    mat.setRow(2, res.getX(), res.getY(), res.getZ(), 0);
    mat.setRow(3, (m_lh.getX())*1000, (m_lh.getY())*1000, (m_lh.getZ())*1000, 1);
    mat_event->setPayload(mat);

    if(m_dev == 0){
        map<string, IEventSink *>::iterator iter = this->registeredEventSinkMap->find("TransfLeft_OUT");
        if (iter != this->registeredEventSinkMap->end()) {
            (*iter).second->push(mat_event);
        } else {
            TFERROR("not found");
        }
    }

}

void KinectARTTransfUnit::RIGHT_HAND_Changed( const KinectEvent * event) {
    KinectJoint receivedJointData = event->getPayload();

    //get kinect coordinats and add translation relative to the art coordinate
	float xl = receivedJointData.getPosition().getX() + translX;
	float yl = receivedJointData.getPosition().getY() + translY;
	float zl = receivedJointData.getPosition().getZ() + translZ;

    //roation by 180 degrees around y, kinect has opposite z-axis
	xl *= -1.0;
	zl *= -1.0;

    //rotation and translation: tracker to cave
	float xl_ = rot_trans[0]*xl + rot_trans[4]*yl + rot_trans[8]*zl + rot_trans[12];
	float yl_ = rot_trans[1]*xl + rot_trans[5]*yl + rot_trans[9]*zl + rot_trans[13];
	float zl_ = rot_trans[2]*xl + rot_trans[6]*yl + rot_trans[10]*zl + rot_trans[14];

    m_rh = Vector3d((1-filterTreshhold)*m_rh.getX() + filterTreshhold*xl_, (1-filterTreshhold)*m_rh.getY() + filterTreshhold*yl_, (1-filterTreshhold)*m_rh.getZ() + filterTreshhold*zl_);

    Vector3d res(m_rh.getX(), m_rh.getY(), m_rh.getZ());
    res.subtract(m_re);

    if(res.getX()*res.getX() + res.getY()*res.getY() + res.getZ()*res.getZ() > 0.0)
        res.normalize();
    else
        return;

    //todo send Matrx4ChangedEvent
    Matrix4ChangedEvent * mat_event = new Matrix4ChangedEvent();

    Matrix4Data mat;
    mat.setRow(0, 0, 0, 0, 0);
    mat.setRow(1, 0, 0, 0, 0);
    mat.setRow(2, res.getX(), res.getY(), res.getZ(), 0);
    mat.setRow(3, (m_rh.getX())*1000, (m_rh.getY())*1000, (m_rh.getZ())*1000, 1);
    mat_event->setPayload(mat);

    map<string, IEventSink *>::iterator iter;

    if(m_dev == 0)
        iter = this->registeredEventSinkMap->find("TransfRight_OUT");
    else if(m_dev == 1)
        iter = this->registeredEventSinkMap->find("TransfTong_OUT");
    else
        iter = this->registeredEventSinkMap->find("TransfStroke_OUT");

    if (iter != this->registeredEventSinkMap->end()) {
        (*iter).second->push(mat_event);
    } else {
        TFERROR("not found");
    }

}

void KinectARTTransfUnit::LEFT_ELBOW_Changed( const KinectEvent * event) {
    KinectJoint receivedJointData = event->getPayload();

	float xl = receivedJointData.getPosition().getX() + translX;
	float yl = receivedJointData.getPosition().getY() + translY;
	float zl = receivedJointData.getPosition().getZ() + translZ;

    //roation by 180 degrees
	xl *= -1.0;
	zl *= -1.0;

    //rotation and translation: tracker to cave
	float xl_ = rot_trans[0]*xl + rot_trans[4]*yl + rot_trans[8]*zl + rot_trans[12];
	float yl_ = rot_trans[1]*xl + rot_trans[5]*yl + rot_trans[9]*zl + rot_trans[13];
	float zl_ = rot_trans[2]*xl + rot_trans[6]*yl + rot_trans[10]*zl + rot_trans[14];

    m_le = Vector3d((1-filterTreshhold)*m_le.getX() + filterTreshhold*xl_, (1-filterTreshhold)*m_le.getY() + filterTreshhold*yl_, (1-filterTreshhold)*m_le.getZ() + filterTreshhold*zl_);

}

void KinectARTTransfUnit::RIGHT_ELBOW_Changed( const KinectEvent * event) {
    KinectJoint receivedJointData = event->getPayload();

	float xl = receivedJointData.getPosition().getX() + translX;
	float yl = receivedJointData.getPosition().getY() + translY;
	float zl = receivedJointData.getPosition().getZ() + translZ;

    //roation by 180 degrees
	xl *= -1.0;
	zl *= -1.0;

    //rotation and translation: tracker to cave
	float xl_ = rot_trans[0]*xl + rot_trans[4]*yl + rot_trans[8]*zl + rot_trans[12];
	float yl_ = rot_trans[1]*xl + rot_trans[5]*yl + rot_trans[9]*zl + rot_trans[13];
	float zl_ = rot_trans[2]*xl + rot_trans[6]*yl + rot_trans[10]*zl + rot_trans[14];

    m_re = Vector3d((1-filterTreshhold)*m_re.getX() + filterTreshhold*xl_, (1-filterTreshhold)*m_re.getY() + filterTreshhold*yl_, (1-filterTreshhold)*m_re.getZ() + filterTreshhold*zl_);
}

void KinectARTTransfUnit::LEFT_HIP_Changed( const KinectEvent * event ) {
    //std::cout << "l hip changed" << std::endl;
}

void KinectARTTransfUnit::RIGHT_HIP_Changed(const KinectEvent * event){
    //std::cout << "r hip changed" << std::endl;
}

void KinectARTTransfUnit::LEFT_SHOULDER_Changed(const KinectEvent * event){
    //std::cout << "l shoulder changed" << std::endl;
}

void KinectARTTransfUnit::RIGHT_SHOULDER_Changed(const KinectEvent * event){
    //std::cout << "r shoulder changed" << std::endl;
}

void KinectARTTransfUnit::LEFT_FOOT_Changed(const KinectEvent * event){
    //std::cout << "l foot changed" << std::endl;
}

void KinectARTTransfUnit::RIGHT_FOOT_Changed(const KinectEvent * event){
    //std::cout << "r foot changed" << std::endl;
}

void KinectARTTransfUnit::LEFT_KNEE_Changed(const KinectEvent * event){
    //std::cout << "l knee changed" << std::endl;
}

void KinectARTTransfUnit::RIGHT_KNEE_Changed(const KinectEvent * event){
    //std::cout << "r knee changed" << std::endl;
}


void KinectARTTransfUnit::GESTURE(const GestureEvent * event){

    Gesture recGesture = event->getPayload();
    std::cout << "gesture arrived with id: " << recGesture << std::endl;


    if(recGesture.getGestureType() == Gesture::Wave){
        std::cout << "wave !!" << std::endl;
        m_dev = (m_dev+1)%3;
    }
    else if(recGesture.getGestureType() == Gesture::Click){
        std::cout << "click !!" << std::endl;

        if(m_dev == 2){
            DigitalChangedEvent * dig_event = new DigitalChangedEvent();

            if(clicked)
                dig_event->setPayload(0);
            else
                dig_event->setPayload(1);

            clicked = !clicked;

            map<string, IEventSink *>::iterator iter = this->registeredEventSinkMap->find("Menu_OUT");

            if (iter != this->registeredEventSinkMap->end()) {
                (*iter).second->push(dig_event);
            } else {
                TFERROR("not found");
            }

        }

    }
    else if(recGesture.getGestureType() == Gesture::Grab){
        std::cout << "grab it !!" << std::endl;
        DigitalChangedEvent * dig_event = new DigitalChangedEvent();

        dig_event->setPayload(1);

        map<string, IEventSink *>::iterator iter;

        if(m_dev == 0){
            iter = this->registeredEventSinkMap->find("Draw_OUT");
            if (iter != this->registeredEventSinkMap->end()) {
                (*iter).second->push(dig_event);
            } else {
                TFERROR("not found");
            }
        }
        else if (m_dev == 1){
            iter = this->registeredEventSinkMap->find("Grab_OUT");
            if (iter != this->registeredEventSinkMap->end()) {
                (*iter).second->push(dig_event);
            } else {
                TFERROR("not found");
            }
        }
        else{
            AnalogChangedEvent * ana_event = new AnalogChangedEvent();

            ana_event->setPayload(1.0f);

            map<string, IEventSink *>::iterator  iter = this->registeredEventSinkMap->find("DrawStroke_OUT");
            if (iter != this->registeredEventSinkMap->end()) {
                (*iter).second->push(ana_event);
            } else {
                TFERROR("not found");
            }
        }

    }
    else if(recGesture.getGestureType() == Gesture::NoMoreGrab){
        std::cout << "no more grab !!" << std::endl;
        DigitalChangedEvent * dig_event = new DigitalChangedEvent();

        dig_event->setPayload(0);

        map<string, IEventSink *>::iterator iter;

        if(m_dev == 0){
            iter = this->registeredEventSinkMap->find("Draw_OUT");
            if (iter != this->registeredEventSinkMap->end()) {
                (*iter).second->push(dig_event);
            } else {
                TFERROR("not found");
            }
        }
        else if (m_dev == 1){
            iter = this->registeredEventSinkMap->find("Grab_OUT");
            if (iter != this->registeredEventSinkMap->end()) {
                (*iter).second->push(dig_event);
            } else {
                TFERROR("not found");
            }
        }
        else{
            AnalogChangedEvent * ana_event = new AnalogChangedEvent();

            ana_event->setPayload(0.0f);

            map<string, IEventSink *>::iterator  iter = this->registeredEventSinkMap->find("DrawStroke_OUT");
            if (iter != this->registeredEventSinkMap->end()) {
                (*iter).second->push(ana_event);
            } else {
                TFERROR("not found");
            }
        }
    }
    else if(recGesture.getGestureType() == Gesture::NoGesture){
        std::cout << "no gesture !!" << std::endl;
    }

}


} //tuiframework
