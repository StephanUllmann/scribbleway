import QtQuick

BaseShape {
    id: root
    
    mode: "none" // Move only, text size is managed via font slider
    shapeIndex: index
    shapeX: model.x
    shapeY: model.y
    shapeWidth: model.width
    shapeHeight: model.height

    onRectGeometryChanged: (nx, ny, nw, nh) => {
        controller.updateShape(index, { "x": nx, "y": ny });
    }

    Text {
        id: textLabel
        x: root.shapeX + 5
        y: root.shapeY + 5
        text: model.text || ""
        color: root.modelColor
        opacity: root.modelOpacity
        font.family: typeof fontFamily !== "undefined" ? fontFamily : controller.defaultFontFamily
        font.pixelSize: typeof fontSize !== "undefined" ? fontSize : 20

        // Sync size changes to the shape model
        onImplicitWidthChanged: syncSize()
        onImplicitHeightChanged: syncSize()
        
        Component.onCompleted: syncSize()

        function syncSize() {
            controller.updateShape(index, { 
                "width": Math.max(50, implicitWidth + 10), 
                "height": Math.max(20, implicitHeight + 10) 
            });
        }
    }

    onDoubleClicked: {
        if (typeof canvasWindow !== "undefined") {
            canvasWindow.startTextEditing(root.shapeIndex);
        }
    }
}
