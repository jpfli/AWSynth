//!APP-HOOK:addMenu
//!MENU-ENTRY:AW Patch Editor

if(!APP.AWPatchEditorInstalled()) {
  APP.log("Adding AW Patch Editor");
  
  // list of file extensions this view can edit
  const extensions = ["AWPATCH"];
  
  // path to the html to load
  // file path will be concatenated after the "?"
  const prefix = `file://${DATA.projectPath}/AWPatchEditor/editor.html?`;
  
  // add extensions for binary files here
  Object.assign(encoding, {
    "AWPATCH":null
  });
  
  class AWPatchEditorView {
    // gets called when the tab is activated
    attach() {
      if(this.DOM.src != prefix + this.buffer.path)
        this.DOM.src = prefix + this.buffer.path;
      this.DOM.contentWindow.readFile = this.readFile;
      this.DOM.contentWindow.saveFile = this.saveFile;
    }
    
    // file was renamed, update iframe
    onRenameBuffer(buffer) {
      if(buffer == this.buffer) {
        this.DOM.src = prefix + this.buffer.path;
      }
    }
    
    readFile(filePath, mode) {
      return window.require("fs").readFileSync(filePath, mode);
    }
    
    saveFile(filePath, data) {
      window.require("fs").writeFileSync(filePath, data);
    }
    
    constructor(frame, buffer) {
      this.buffer = buffer;
      this.DOM = DOC.create(frame, "iframe", {
        className:"AWPatchEditorView",
        src: prefix + buffer.path,
        style: {
          border: "0px none",
          width: "100%",
          height: "100%",
          margin: 0,
          position: "absolute"
        },
        load:function() {
        }
      });
    }
  }
  
  APP.add(new class AWPatchEditor {
  
    AWPatchEditorInstalled(){ return true; }
    
    pollViewForBuffer(buffer, vf) {
      if(extensions.indexOf(buffer.type) != -1 && vf.priority < 2) {
        vf.view = AWPatchEditorView;
        vf.priority = 2;
      }
    }
  
  }());
}

