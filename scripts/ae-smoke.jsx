(function () {
    var root = new File($.fileName).parent.parent;
    var folder = new Folder(root.fsName + '/test-output');
    var log = new File(folder.fsName + '/ae-smoke.log');
    function write(s) { $.writeln(s); try { if(log.open('a')) { log.writeln(s); log.close(); } } catch(ignore) {} }
    function renderPNG(comp, name) {
        var item=app.project.renderQueue.items.add(comp);
        item.timeSpanStart=0;item.timeSpanDuration=comp.frameDuration;
        var om=item.outputModule(1), found=false;
        for(var t=0;t<om.templates.length;t++) if(/PNG/i.test(om.templates[t])) { om.applyTemplate(om.templates[t]);found=true;break; }
        if(!found)throw new Error('No PNG output template: '+om.templates.join(', '));
        try { om.setSettings({'Channels':'RGB + Alpha'}); } catch(ignore) {}
        om.file=new File(folder.fsName+'/'+name+'_[#####].png');
        app.project.renderQueue.render();
        if(item.status!==RQItemStatus.DONE)throw new Error('Render failed: '+name);
        item.remove();
    }
    try {
        write('START ' + app.version);
        // This script is launched in a separate AE instance. Never replace existing work.
        if (app.project && app.project.numItems > 0) throw new Error('Expected an empty test instance; refusing to change an existing project.');
        app.beginSuppressDialogs();
        var source = app.project.importFile(new ImportOptions(new File(folder.fsName + '/source.png')));
        var comp = app.project.items.addComp('CurveSmear - Study 02', 800, 520, 1, 2, 24);
        var layer = comp.layers.add(source);
        var mask = layer.property('ADBE Mask Parade').addProperty('ADBE Mask Atom');
        mask.name = 'Flow - edit this open path'; mask.maskMode = MaskMode.NONE;
        var shape = new Shape();
        shape.vertices = [[365,220],[700,310]];
        shape.inTangents = [[0,0],[-160,40]];
        shape.outTangents = [[95,-30],[0,0]];
        shape.closed = false;
        mask.property('ADBE Mask Shape').setValue(shape);
        var fx = layer.property('ADBE Effect Parade').addProperty('Siosi CurveSmear');
        if (!fx) throw new Error('CurveSmear effect not found');
        fx.property(1).setValue(1);
        for (var i=1; i<=fx.numProperties; i++) write(i + ': ' + fx.property(i).name + ' = ' + fx.property(i).value);
        comp.openInViewer();
        app.project.bitsPerChannel = 8;
        renderPNG(comp,'ae8'); write('RENDER 8 OK');
        fx.property(2).setValue(0);
        renderPNG(comp,'ae-zero'); write('ZERO OK');
        fx.property(2).setValue(250);
        app.project.bitsPerChannel = 16;
        renderPNG(comp,'ae16'); write('RENDER 16 OK');
        app.project.bitsPerChannel = 32;
        renderPNG(comp,'ae32'); write('RENDER 32 OK');
        app.project.bitsPerChannel = 8;
        app.project.save(new File(folder.fsName + '/CurveSmear-test.aep'));
        write('SUCCESS');
        app.endSuppressDialogs(false);
    } catch (e) { write('ERROR ' + e.toString() + ' line ' + e.line); app.endSuppressDialogs(false); alert('CurveSmear test: ' + e.toString() + ' line ' + e.line); }
})();
