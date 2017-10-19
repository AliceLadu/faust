/*
 faust2wasm
 Additional code: GRAME 2017
*/
 
'use strict';

if (typeof (AudioWorkletNode) === "undefined") {
	alert("AudioWorklet is not supported in this browser !")
}

var faust = faust || {};

class mydspNode extends AudioWorkletNode {
    
    constructor(context, options) {
        
        var json_object = JSON.parse(getJSONmydsp());
        
        // Setting values for the input, the output and the channel count.
        options.numberOfInputs = parseInt(json_object.inputs);
        options.numberOfOutputs = parseInt(json_object.outputs);
        options.channelCount = 1;
        
        super(context, 'mydsp', options);
        
        // JSON parsing functions
        this.parse_ui = function(ui, obj)
        {
            for (var i = 0; i < ui.length; i++) {
                this.parse_group(ui[i], obj);
            }
        }
        
        this.parse_group = function(group, obj)
        {
            if (group.items) {
                this.parse_items(group.items, obj);
            }
        }
        
        this.parse_items = function(items, obj)
        {
            for (var i = 0; i < items.length; i++) {
            	this.parse_item(items[i], obj);
            }
        }
        
        this.parse_item = function(item, obj)
        {
            if (item.type === "vgroup"
                || item.type === "hgroup"
                || item.type === "tgroup") {
                this.parse_items(item.items, obj);
            } else if (item.type === "hbargraph"
                       || item.type === "vbargraph") {
                // Keep bargraph adresses
                obj.outputs_items.push(item.address);
            } else if (item.type === "vslider"
                       || item.type === "hslider"
                       || item.type === "button"
                       || item.type === "checkbox"
                       || item.type === "nentry") {
                // Keep inputs adresses
                obj.inputs_items.push(item.address);
            }
        }
   
        this.json_object = json_object;
        
        // input/output items
        this.inputs_items = [];
        this.outputs_items = [];
       
        // Parse UI
        this.parse_ui(this.json_object.ui, this);
        
        //console.log(this.inputs_items);
        //console.log(this.outputs_items);
        
        // Start node/processor communication port
        //this.port.start();
    }
    
    getJSON()
    {
        return getJSONmydsp();
    }
    
    setParamValue(path, val)
    {
        //console.log(this);
        //this.parameters.get(path).value = val;
    }
    
    setOutputParamHandler(handler)
    {
        // TODO
    }
    
    // TO REMOVE
    getNumInputs()
    {
        return parseInt(this.json_object.inputs);
    }
    
    getNumOutputs()
    {
        return parseInt(this.json_object.outputs);
    }
    
    getParams()
    {
        return [];
    }
    
}

faust.createmydsp = function(callback)
{
    // The main global scope
    window.audioWorklet.addModule("mydsp-processor.js")
    .then(function () {
         audio_context = new AudioContext();
         callback(new mydspNode(audio_context, {}));
    })
    .catch(function() { console.log("Faust mydsp cannot be loaded or compiled"); });
}

