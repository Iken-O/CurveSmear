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
        if (!fx.property('Source Matte') || !fx.property('Matte Channel')) throw new Error('Top matte controls are missing');
        fx.property('Flow Path (open mask)').setValue(1);mark('path');fx.property('Smear Amount').setValue(180);mark('amount');fx.property('Radius').setValue(60);mark('radius');
        if (fx.property('End Caps').value !== 2) throw new Error('End Caps default is not Flat');
        if (fx.property('Sampling').value !== 2) throw new Error('Sampling default is not Nearest');
        if (fx.property('Matte Channel').value !== 1) throw new Error('Matte Channel default is not Luminance');
        if (!fx.property('Width Profile')) throw new Error('Width Profile group is missing');
        if (fx.property('Invert Matte')) throw new Error('Invert Matte was not removed');
        mark('defaults');
        comp.openInViewer();layer.selected=true;mark('PASS');
    } catch(e) { mark('FAIL '+e.toString()+' line '+e.line); }
    log.close();
    app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);
})();
