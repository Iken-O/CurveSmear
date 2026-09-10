/* Adds a self-contained 1080p demo to the current project. Does not replace it. */
(function () {
    var root=new File($.fileName).parent.parent;
    var file=new File(root.fsName+'/assets/demo-source.png');
    if(!file.exists){alert('Missing assets/demo-source.png. Keep the distribution folders together.');return;}
    app.beginUndoGroup('Create CurveSmear Demo');
    try {
        var source=app.project.importFile(new ImportOptions(file));
        var comp=app.project.items.addComp('CurveSmear - 1080p Demo',1920,1080,1,3,24);
        var layer=comp.layers.add(source);
        var mask=layer.property('ADBE Mask Parade').addProperty('ADBE Mask Atom');
        mask.name='Flow - edit this path';mask.maskMode=MaskMode.NONE;
        var shape=new Shape();shape.closed=false;
        shape.vertices=[[890,460],[1560,640]];
        shape.inTangents=[[0,0],[-320,80]];
        shape.outTangents=[[190,-60],[0,0]];
        mask.property('ADBE Mask Shape').setValue(shape);
        var fx=layer.property('ADBE Effect Parade').addProperty('Siosi CurveSmear');
        fx.property(1).setValue(1);fx.property(2).setValue(500);fx.property(3).setValue(130);
        comp.openInViewer();layer.selected=true;mask.property('ADBE Mask Shape').selected=true;
    } catch(e) { alert('CurveSmear demo: '+e.toString()+'\nInstall CurveSmear.aex and restart After Effects first.'); }
    finally { app.endUndoGroup(); }
})();
