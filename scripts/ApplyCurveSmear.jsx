/* Apply to one selected footage/precomp layer. All changes are one undo step. */
(function () {
    var comp = app.project.activeItem;
    if (!(comp instanceof CompItem) || comp.selectedLayers.length !== 1) {
        alert('Select one image or precomp layer, then run ApplyCurveSmear.'); return;
    }
    var layer = comp.selectedLayers[0];
    var effects = layer.property('ADBE Effect Parade');
    if (!effects || !effects.canAddProperty('Siosi CurveSmear')) {
        alert('CurveSmear is not installed. Install CurveSmear.aex and restart After Effects.'); return;
    }
    var masks = layer.property('ADBE Mask Parade'), maskIndex = 0;
    if (!masks) { alert('This layer does not support mask paths.'); return; }
    // Reuse a selected open mask; otherwise create a new independent flow mask.
    for (var i=1; i<=masks.numProperties; i++) {
        var candidate = masks.property(i);
        if ((candidate.selected || candidate.property('ADBE Mask Shape').selected) && !candidate.property('ADBE Mask Shape').value.closed) { maskIndex=i; break; }
    }
    app.beginUndoGroup('Apply CurveSmear');
    try {
        if (!maskIndex) {
            var w=layer.width,h=layer.height;
            var mask=masks.addProperty('ADBE Mask Atom');
            mask.name='CurveSmear Flow'; mask.maskMode=MaskMode.NONE;
            var shape=new Shape();shape.closed=false;
            shape.vertices=[[w*.35,h*.5],[w*.8,h*.5]];
            shape.inTangents=[[0,0],[-w*.15,h*.12]];
            shape.outTangents=[[w*.15,-h*.12],[0,0]];
            mask.property('ADBE Mask Shape').setValue(shape);maskIndex=mask.propertyIndex;
        } else { masks.property(maskIndex).maskMode=MaskMode.NONE; }
        var fx=effects.addProperty('Siosi CurveSmear');fx.property(1).setValue(maskIndex);
        fx.property(2).setValue(Math.min(layer.width,layer.height)*.35);
        fx.property(3).setValue(Math.min(layer.width,layer.height)*.10);
        masks.property(maskIndex).property('ADBE Mask Shape').selected=true;
    } catch(e) { alert('CurveSmear: '+e.toString()); }
    finally { app.endUndoGroup(); }
})();
