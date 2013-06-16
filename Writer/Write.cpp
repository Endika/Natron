//  Powiter
//
//  Created by Alexandre Gauthier-Foichat on 06/12
//  Copyright (c) 2013 Alexandre Gauthier-Foichat. All rights reserved.
//  contact: immarespond at gmail dot com

#include "Write.h"
#include "Core/row.h"
#include "Writer/Writer.h"
#include "Core/lookUpTables.h"
#include "Gui/knob.h"

/*Constructors should initialize variables, but shouldn't do any heavy computations, as these objects
 are oftenly re-created. To initialize the input color-space , you can do so by overloading
 initializeColorSpace. This function is called after the constructor and before any
 reading occurs.*/
Write::Write(Writer* writer):op(writer),_lut(0),_premult(false),_optionalKnobs(0){
    
}

Write::~Write(){
    
}


void Write::writeAndDelete(){
    writeAllData();
    delete this;
}

void Write::to_byte(Channel z, uchar* to, const float* from, const float* alpha, int W, int delta ){
    if( z <= 3 && !_lut->linear()){
        if(alpha && _premult){
            _lut->to_byte(to, from, alpha, W,delta);
        }else{
            _lut->to_byte(to, from, W,delta);
        }
    }else{
        Linear::to_byte(to, from, W,delta);
    }
}
void Write::to_short(Channel z, U16* to, const float* from, const float* alpha, int W, int bits , int delta){
    if( z <= 3 && !_lut->linear()){
        if(alpha && _premult){
            _lut->to_short(to, from, alpha, W,delta);
        }else{
            _lut->to_short(to, from, W,delta);
        }
    }else{
        Linear::to_short(to, from, W,delta);
    }
}
void Write::to_float(Channel z, float* to, const float* from, const float* alpha, int W, int delta ){
    if( z <= 3 && !_lut->linear()){
        if(alpha && _premult){
            _lut->to_float(to, from, alpha, W,delta);
        }else{
            _lut->to_float(to, from, W,delta);
        }
    }else{
        Linear::to_float(to, from, W,delta);
    }
}

void WriteKnobs::initKnobs(Knob_Callback* callback,std::string& fileType){
    
    _op->createKnobDynamically();
}

