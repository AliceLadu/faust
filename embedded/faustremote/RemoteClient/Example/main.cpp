//
//  main.cpp
//  
//
//  Created by Sarah Denoux on 30/05/13.
//  Copyright (c) 2013 __MyCompanyName__. All rights reserved.
//
//
// MAIN OF DYNAMIC COMPILER
//
//-------------------------------------------------------------------------
// 									MAIN
//-------------------------------------------------------------------------
#include <sys/stat.h>
#include <iostream>

#include "../../utilities.h"
#include "faust/remote-dsp.h"
#include "faust/gui/faustqt.h"
#include "faust/audio/coreaudio-dsp.h"
#include "faust/audio/jack-dsp.h"

std::list<GUI*> GUI::fGuiList;

int main(int argc, char* argv[])
{
    list<QTGUI*>                listInterfaces;
    list<remote_dsp*>            listDSPs;
    list<remote_dsp_factory*>   listFactories;
    list<audio*>                listAudios;
    
    if(argv[1] && !strcmp(argv[1], "--help")){
        printf("\nOPTIONS OF FAUST DISTRIBUTED : \n\n\
               ########### REMOTE CALCULATION PARAMETERS ############\n\
               --ipserver ==> default is localhost\n\
               --portserver ==> default is 7777 \n\
               --frequency ==> default is 44100 \n\
               --buffer ==> default is 512 \n\n\
               ########### NET JACK PARAMETER #######################\n\
               --NJ_ip ==> MULTICAST_DEFAULT_IP \n\
               --NJ_port ==> MULTICAST_DEFAULT_PORT\n\
               --NJ_compression ==> default is -1\n\
               --NJ_mtu ==> default is 1500\n\
               --NJ_latency ==>default is 2 \n\
               ########### DSP TO BE REMOTLY COMPILED ###############\n\
               --file ==> no DSP = no application\n\
               --n    ==> indicates the number of instances of DSP \n\n\
               You can also add faust compilation options\n\
               -vec/-sch/...\n\
               \n");
        return 0;
    }
    
    string ipServer = loptions(argv, "--ipserver", "localhost");
    int portServer = lopt(argv, "--portserver", 7777);
    
    int srate = lopt(argv, "--frequency", 44100);
    int	fpb = lopt(argv, "--buffer", 512);
    
    printf("ip = %i || %i\n", srate, fpb);
    
    QApplication myApp(argc, argv);
    
    for(int i =0; i<argc; i++){
        
        char filePath[256];
        
        int numberInstances = lopt_Spe(i, argv, "--file", filePath);
        
        if(numberInstances != 0){
            if(numberInstances > 1) i+=2;
            
            string errorFactory("");
            
            const char* arguments[argc];
            int nbArgument = 0;
            
            for(int i=1; i<argc; i++){
                
                if(string(argv[i]).find("--")!=string::npos){
                    i ++;
                }
                else{
                    arguments[nbArgument] = argv[i];
                    nbArgument++;
                }
            }
            
            string content = pathToContent(filePath);
            
            remote_dsp_factory* factory = createRemoteDSPFactoryFromString("FaustRemote", content, nbArgument, arguments, ipServer, portServer, errorFactory, 3);
            
            if(factory != NULL){
                
                listFactories.push_back(factory);
                
                for(int j = 0; j<numberInstances; j++){
                    
                    int errorInstance;
                    
                    remote_dsp* DSP = createRemoteDSPInstance(factory, argc, (const char**)(argv), srate, fpb, NULL, NULL, errorInstance);
                    
                    if(DSP != NULL){
                        
                        QTGUI* interface = new QTGUI();
                        listInterfaces.push_back(interface);
                        
                        listDSPs.push_back(DSP);
                        
                        jackaudio* audio = new jackaudio;
//                        coreaudio* audio = new coreaudio(srate, fpb);
                        listAudios.push_back(audio);
                        
                        DSP->buildUserInterface(interface);   
                        
                        if(!audio->init("Test", DSP))
                            break;
                        else
                            printf("INIT\n");
                        
                        if(!audio->start())
                            break;
                        else
                            printf("START\n");
                        
                        interface->run();
                        
                    }
                    else{
                        printf("CREATE INSTANCE FAILED = %d\n", errorInstance);
                    }
                }
            }
            else{
                printf("CREATE FACTORY FAILED = %s\n", errorFactory.c_str());
            }
        }
    }
    
    myApp.setStyleSheet(STYLESHEET);
    myApp.exec();
    
    
    //  STOP && DESALLOCATION OF ALL RESOURCES
    list<QTGUI*>::iterator itI = listInterfaces.begin();
    list<remote_dsp*>::iterator itD = listDSPs.begin();
    list<remote_dsp_factory*>::iterator itF = listFactories.begin();
    list<audio*>::iterator itA = listAudios.begin();
    
    while(itI != listInterfaces.end()){
        (*itI)->stop();
        itI++;
    }
    listInterfaces.clear();
    
    while(itA != listAudios.end()){
        (*itA)->stop();
        itA++;
    }
    listAudios.clear();
    
    
    while(itA != listAudios.end()){
        (*itA)->stop();
        itA++;
    }
    listAudios.clear();
    
    
    while(itD != listDSPs.end()){
        deleteRemoteDSPInstance(*itD);
        itD++;
    }
    listDSPs.clear();
    
    
    while(itF != listFactories.end()){
        deleteRemoteDSPFactory(*itF);
        itF++;
    }
    listFactories.clear();
    
  	return 0;
}


