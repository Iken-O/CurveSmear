(function () {
    var root = new File($.fileName).parent.parent;
    var folder = new Folder(root.fsName + '/test-output');
    var log = new File(folder.fsName + '/ae-smoke.log');
    var suppress = false, ownsProject = false;
    function write(s) { $.writeln(s); try { if(log.open('a')) { log.writeln(s); log.close(); } } catch(ignore) {} }
    function renderStill(comp, name) {
        var previous = new File(folder.fsName+'/'+name+'_00000.tif');
        if (previous.exists) previous.remove();
        var item=app.project.renderQueue.items.add(comp);
        item.timeSpanStart=0;item.timeSpanDuration=comp.frameDuration;
        var om=item.outputModule(1), found=false;
        for(var t=0;t<om.templates.length;t++) if(/TIFF/i.test(om.templates[t])) { om.applyTemplate(om.templates[t]);found=true;break; }
        if(!found)throw new Error('No TIFF output template: '+om.templates.join(', '));
        try { om.setSettings({'Channels':'RGB + Alpha'}); } catch(ignore) {}
        om.file=new File(folder.fsName+'/'+name+'_[#####].tif');
        app.project.renderQueue.render();
        if(item.status!==RQItemStatus.DONE)throw new Error('Render failed: '+name);
        item.remove();
    }
    try {
        write('START ' + app.version);
        // This script is launched in a separate AE instance. Never replace existing work.
        if (app.project && app.project.numItems > 0) throw new Error('Expected an empty test instance; refusing to change an existing project.');
        ownsProject = true;
        app.beginSuppressDialogs(); suppress = true;
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
        // Render once in the exact state users see immediately after adding the effect.
        // No flow path is selected yet; this must be a clean pass-through.
        renderStill(comp,'ae-no-path'); write('NO PATH OK');
        fx.property('Flow Path (open mask)').setValue(1);
        for (var i=1; i<=fx.numProperties; i++) {
            var value = '<no value>';
            try { value = fx.property(i).value; } catch (ignoreValue) {}
            write(i + ': ' + fx.property(i).name + ' = ' + value);
        }
        comp.openInViewer();
        app.project.bitsPerChannel = 8;
        renderStill(comp,'ae8'); write('RENDER 8 OK');
        var closedShape = mask.property('ADBE Mask Shape').value;
        closedShape.closed = true;
        mask.property('ADBE Mask Shape').setValue(closedShape);
        renderStill(comp,'ae-closed-path'); write('CLOSED PATH PASS-THROUGH OK');
        closedShape.closed = false;
        mask.property('ADBE Mask Shape').setValue(closedShape);
        renderStill(comp,'ae-reopened-path'); write('REOPENED PATH OK');
        fx.property('Smear Amount').setValue(0);
        renderStill(comp,'ae-zero'); write('ZERO OK');
        fx.property('Smear Amount').setValue(250);
        app.project.bitsPerChannel = 16;
        renderStill(comp,'ae16'); write('RENDER 16 OK');
        app.project.bitsPerChannel = 32;
        renderStill(comp,'ae32'); write('RENDER 32 OK');
        app.project.bitsPerChannel = 8;
        app.project.save(new File(folder.fsName + '/CurveSmear-test.aep'));
        write('SUCCESS');
    } catch (e) { write('ERROR ' + e.toString() + ' line ' + e.line); }
    finally {
        if (suppress) app.endSuppressDialogs(false);
        if (ownsProject && app.project) app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);
    }
})();
