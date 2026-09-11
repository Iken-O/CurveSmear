/* Runtime smoke test. Writes each completed step to test-output/ae-smoke.log. */
(function () {
    var root=new File($.fileName).parent.parent;
    var log=new File(root.fsName+'/test-output/ae-smoke.log');
    log.open('w');
    function mark(s){log.writeln(s);}
    try {
        mark('start');
        var comp=app.project.items.addComp('CurveSmear Smoke Test',640,360,1,1,24);mark('comp');
        var layer=comp.layers.addSolid([1,1,1],'Source',640,360,1,1);mark('layer');
        var mask=layer.property('ADBE Mask Parade').addProperty('ADBE Mask Atom');mask.maskMode=MaskMode.NONE;mark('mask');
        var shape=new Shape();shape.closed=false;shape.vertices=[[120,180],[520,180]];shape.inTangents=[[0,0],[-100,60]];shape.outTangents=[[100,-60],[0,0]];mask.property('ADBE Mask Shape').setValue(shape);mark('shape');
        var parade=layer.property('ADBE Effect Parade');mark('canAdd='+parade.canAddProperty('Siosi CurveSmear'));
        var fx=parade.addProperty('Siosi CurveSmear');mark('effect '+fx.numProperties);
        if (fx.numProperties < 20) throw new Error('CurveSmear 0.1.4 parameters are missing');
        fx.property(1).setValue(1);mark('path');fx.property(2).setValue(180);mark('amount');fx.property(3).setValue(60);mark('radius');
        if (fx.property(11).value !== 2) throw new Error('End Caps default is not Flat');
        if (fx.property(12).value !== 2) throw new Error('Sampling default is not Nearest');
        if (fx.property(14).value !== 1) throw new Error('Smooth Profile default is off');
        if (fx.property(19).value !== 1) throw new Error('Matte Channel default is not Luminance');
        if (fx.property(20).value !== 0) throw new Error('Invert Matte default is on');
        mark('defaults');
        comp.openInViewer();layer.selected=true;mark('PASS');
    } catch(e) { mark('FAIL '+e.toString()+' line '+e.line); }
    log.close();
    app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);
})();
