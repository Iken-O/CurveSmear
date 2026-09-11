/* Source Matte render test. Outputs TIFFs and a step log in test-output/. */
(function () {
    var root = new File($.fileName).parent.parent;
    var folder = new Folder(root.fsName + '/test-output');
    if (!folder.exists) folder.create();
    var log = new File(folder.fsName + '/ae-matte-smoke.log');
    log.open('w');
    function mark(s) { log.writeln(s); }
    function renderStill(comp, name) {
        var item = app.project.renderQueue.items.add(comp);
        item.timeSpanStart = 0;
        item.timeSpanDuration = comp.frameDuration;
        var output = item.outputModule(1), found = false;
        for (var i = 0; i < output.templates.length; i++) {
            if (/TIFF/i.test(output.templates[i])) { output.applyTemplate(output.templates[i]); found = true; break; }
        }
        if (!found) throw new Error('No TIFF output template');
        output.file = new File(folder.fsName + '/' + name + '_[#####].tif');
        app.project.renderQueue.render();
        if (item.status !== RQItemStatus.DONE) throw new Error('Render failed: ' + name);
        item.remove();
        mark(name);
    }
    try {
        app.beginSuppressDialogs();
        var footage = app.project.importFile(new ImportOptions(new File(folder.fsName + '/source.png')));
        var comp = app.project.items.addComp('CurveSmear Source Matte Test', 800, 520, 1, 1, 24);
        var source = comp.layers.add(footage);
        var mask = source.property('ADBE Mask Parade').addProperty('ADBE Mask Atom');
        mask.maskMode = MaskMode.NONE;
        var shape = new Shape();
        shape.closed = false;
        shape.vertices = [[365,220],[700,310]];
        shape.inTangents = [[0,0],[-160,40]];
        shape.outTangents = [[95,-30],[0,0]];
        mask.property('ADBE Mask Shape').setValue(shape);
        var fx = source.property('ADBE Effect Parade').addProperty('Siosi CurveSmear');
        fx.property(1).setValue(1);
        fx.property(2).setValue(0);
        renderStill(comp, 'matte-baseline');
        fx.property(2).setValue(250);
        var black = comp.layers.addSolid([0,0,0], 'Black Source Matte', 800, 520, 1, 1);
        var white = comp.layers.addSolid([1,1,1], 'White Source Matte', 800, 520, 1, 1);
        black.enabled = false;
        white.enabled = false;
        fx.property(18).setValue(black.index);
        fx.property(19).setValue(1);
        fx.property(20).setValue(0);
        renderStill(comp, 'matte-black-luma');
        fx.property(18).setValue(white.index);
        renderStill(comp, 'matte-white-luma');
        fx.property(18).setValue(black.index);
        fx.property(20).setValue(1);
        renderStill(comp, 'matte-black-inverted');
        fx.property(20).setValue(0);
        fx.property(19).setValue(2);
        renderStill(comp, 'matte-black-alpha');
        mark('PASS');
    } catch (e) {
        mark('FAIL ' + e.toString() + ' line ' + e.line);
    }
    log.close();
    app.endSuppressDialogs(false);
    app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);
}());
