function attachHoverHighlight(plotDiv) {
    const DEFAULT_EDGE_OPACITY = 0.2;
    const HIGHLIGHT_OPACITY = 1.0;
    const FADE_OPACITY = 0.1;
    const NODE_OPACITY = 1.0;

    plotDiv.on('plotly_hover', function(evt) {
        const targetID = evt.points[0].customdata;

        let update = { opacity: [] };

        plotDiv.data.forEach((trace, idx) => {
            // Keep nodes and headers always fully visible
            if (trace.name === 'nodes' || trace.name === 'headers') {
                update.opacity[idx] = NODE_OPACITY;
                return;
            }

            // For edges/chunks: highlight only the ones with same edge_id
            if (trace.customdata && trace.customdata.includes(targetID)) {
                update.opacity[idx] = HIGHLIGHT_OPACITY;
            } else {
                update.opacity[idx] = FADE_OPACITY;
            }
        });

        Plotly.restyle(plotDiv, update);
    });

    plotDiv.on('plotly_unhover', function() {
        let update = { opacity: [] };

        plotDiv.data.forEach((trace, idx) => {
            if (trace.name === 'nodes' || trace.name === 'headers') {
                update.opacity[idx] = NODE_OPACITY;          // stay bright
            } else {
                update.opacity[idx] = DEFAULT_EDGE_OPACITY;  // reset edges/chunks
            }
        });

        Plotly.restyle(plotDiv, update);
    });
}
