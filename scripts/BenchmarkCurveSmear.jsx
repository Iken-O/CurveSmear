/* Run in an empty, separate AE instance. Measures pipeline wall time, including TIFF writing.
   The native benchmark uses analytic sine curves; these host cases use cubic Bezier masks.
   Their timing numbers are not directly interchangeable. */
(function () {
    var root = new File($.fileName).parent.parent;
    var folder = new Folder(root.fsName + '/test-output/ae-benchmark');
    if (!folder.exists) folder.create();
    var log = new File(folder.fsName + '/results.csv');
    function append(line) { if (!log.open('a')) throw new Error('Cannot write benchmark log'); log.writeln(line); log.close(); }
    if (app.project && app.project.numItems > 0) { append('REFUSED: expected empty project'); return; }
    var suppress = false;
    try {
        log.open('w'); log.writeln('ae_version,mfr,case,repeat,bpc,frames,wall_ms,wall_ms_per_frame'); log.close();
        app.beginSuppressDialogs(); suppress = true;
        var frames = 6;
        var comp = app.project.items.addComp('CurveSmear Benchmark 1080p', 1920, 1080, 1, frames / 24, 24);
        var source = app.project.importFile(new ImportOptions(new File(folder.fsName+'/source.png')));
        var layer = comp.layers.add(source);
        var mask = layer.property('ADBE Mask Parade').addProperty('ADBE Mask Atom'); mask.maskMode = MaskMode.NONE;
        var shape = new Shape(); shape.closed = false;
        shape.vertices = [[700,540],[1200,540]];
        shape.inTangents = [[0,0],[-166.6667,-133.3333]];
        shape.outTangents = [[166.6667,-133.3333],[0,0]];
        mask.property('ADBE Mask Shape').setValue(shape);
        var fx = layer.property('ADBE Effect Parade').addProperty('Siosi CurveSmear');
        fx.property('Flow Path (open mask)').setValue(1);
        fx.property('Radius').setValue(65);
        // Different effect state on each frame, avoiding an identical-frame result cache hit.
        for (var f = 0; f < frames; f++) fx.property('Seed').setValueAtTime(f/24, f);
        var matteFootage = app.project.importFile(new ImportOptions(new File(folder.fsName+'/matte.png')));
        var matte = comp.layers.add(matteFootage);matte.enabled=false;
        function render(mode, repeat, name, bpc, amount, sampling, useMatte) {
            app.project.bitsPerChannel = bpc;
            fx.property('Smear Amount').setValue(amount);
            fx.property('Sampling').setValue(sampling);
            fx.property('Source Matte').setValue(useMatte ? matte.index : 0);
            var item = app.project.renderQueue.items.add(comp);
            item.timeSpanStart=0;item.timeSpanDuration=frames/24;
            var om=item.outputModule(1), found=false;
            for(var i=0;i<om.templates.length;i++) if(/TIFF/i.test(om.templates[i])){om.applyTemplate(om.templates[i]);found=true;break;}
            if(!found)throw new Error('TIFF template unavailable');
            om.file=new File(folder.fsName+'/'+mode+'_'+name+'_r'+repeat+'_[#####].tif');
            app.purge(PurgeTarget.ALL_CACHES);
            var start=new Date().getTime();app.project.renderQueue.render();var ms=new Date().getTime()-start;
            if(item.status!==RQItemStatus.DONE)throw new Error('Render failed '+name);
            append(app.version+','+mode+','+name+','+repeat+','+bpc+','+frames+','+ms+','+(ms/frames));item.remove();
        }
        for(var mode=0;mode<2;mode++) {
            // AE restores the pre-script MFR configuration after the script finishes.
            app.setMultiFrameRenderingConfig(mode===1,100);
            for(var repeat=0;repeat<3;repeat++) {
                render(mode?'on':'off',repeat,'zero_8',8,0,2,false);
                render(mode?'on':'off',repeat,'short_8_nearest',8,250,2,false);
                render(mode?'on':'off',repeat,'short_8_linear_matte',8,250,1,true);
                render(mode?'on':'off',repeat,'short_32_linear_matte',32,250,1,true);
            }
        }
        // Make the project reusable for host profiler and separate aerender runs.
        app.project.save(new File(folder.fsName+'/CurveSmear-Benchmark.aep'));
        append('DONE');
        app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);
    } catch(e) { append('ERROR '+e.toString()+' line '+e.line); }
    finally { if(suppress) app.endSuppressDialogs(false); }
}());
