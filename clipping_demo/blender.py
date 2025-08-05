import bpy
import bmesh
from mathutils import Vector
import random

def split_mesh_by_vertex_limit(obj, max_verts=128):
    """
    Split a mesh object into multiple objects with vertex count limit
    
    Args:
        obj: The mesh object to split
        max_verts: Maximum vertices per submesh (default: 128)
    """
    if obj.type != 'MESH':
        print(f"Object {obj.name} is not a mesh")
        return []
    
    # Get the mesh data
    mesh = obj.data
    
    # Create a bmesh object
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.verts.ensure_lookup_table()
    bm.faces.ensure_lookup_table()
    
    # Enable UV layers
    uv_layer = bm.loops.layers.uv.active
    
    # Store original object properties
    original_name = obj.name
    original_location = obj.location.copy()
    original_rotation = obj.rotation_euler.copy()
    original_scale = obj.scale.copy()
    
    # Create list to store new objects
    new_objects = []
    
    # Keep track of processed faces
    processed_faces = set()
    
    # Counter for naming
    submesh_counter = 0
    
    while len(processed_faces) < len(bm.faces):
        # Create a new bmesh for the submesh
        sub_bm = bmesh.new()
        
        # Copy UV layers structure
        for uv in bm.loops.layers.uv:
            sub_bm.loops.layers.uv.new(uv.name)
        
        # Get UV layer reference for the new bmesh
        sub_uv_layer = sub_bm.loops.layers.uv.active
        
        # Dictionary to map old verts to new verts
        vert_map = {}
        
        # Set to store faces for this submesh
        current_faces = set()
        current_vert_count = 0
        
        # Find a starting face that hasn't been processed
        start_face = None
        for face in bm.faces:
            if face.index not in processed_faces:
                start_face = face
                break
        
        if start_face is None:
            break
        
        # Queue for BFS traversal
        face_queue = [start_face]
        queued_faces = {start_face.index}
        
        # Build submesh using BFS to keep connected geometry together
        while face_queue and current_vert_count < max_verts:
            face = face_queue.pop(0)
            
            # Check if adding this face would exceed vertex limit
            new_verts_needed = sum(1 for v in face.verts if v.index not in vert_map)
            if current_vert_count + new_verts_needed > max_verts:
                continue
            
            # Add face to current submesh
            current_faces.add(face.index)
            processed_faces.add(face.index)
            
            # Create vertices if they don't exist
            new_face_verts = []
            for vert in face.verts:
                if vert.index not in vert_map:
                    new_vert = sub_bm.verts.new(vert.co)
                    vert_map[vert.index] = new_vert
                    current_vert_count += 1
                new_face_verts.append(vert_map[vert.index])
            
            # Create the face
            try:
                new_face = sub_bm.faces.new(new_face_verts)
                
                # Copy UV coordinates if they exist
                if uv_layer and sub_uv_layer:
                    for i, loop in enumerate(face.loops):
                        new_loop = new_face.loops[i]
                        # Copy all UV layers
                        for uv_name in bm.loops.layers.uv.keys():
                            src_uv = bm.loops.layers.uv[uv_name]
                            dst_uv = sub_bm.loops.layers.uv[uv_name]
                            new_loop[dst_uv].uv = loop[src_uv].uv
                
                # Copy face smooth shading
                new_face.smooth = face.smooth
                
                # Copy material index
                new_face.material_index = face.material_index
                
            except ValueError:
                # Face might already exist, skip
                pass
            
            # Add neighboring faces to queue
            for edge in face.edges:
                for linked_face in edge.link_faces:
                    if (linked_face.index not in processed_faces and 
                        linked_face.index not in queued_faces):
                        face_queue.append(linked_face)
                        queued_faces.add(linked_face.index)
        
        # If we have vertices, create a new object
        if len(sub_bm.verts) > 0:
            # Update the bmesh
            sub_bm.verts.ensure_lookup_table()
            sub_bm.faces.ensure_lookup_table()
            
            # Create new mesh
            new_mesh = bpy.data.meshes.new(f"{original_name}_part_{submesh_counter:03d}")
            
            # Transfer the bmesh to the new mesh
            sub_bm.to_mesh(new_mesh)
            sub_bm.free()
            
            # Create new object
            new_obj = bpy.data.objects.new(new_mesh.name, new_mesh)
            new_obj.location = original_location
            new_obj.rotation_euler = original_rotation
            new_obj.scale = original_scale
            
            # Copy materials
            for mat in obj.data.materials:
                new_obj.data.materials.append(mat)
            
            # Copy custom split normals if they exist
            if mesh.has_custom_normals:
                new_mesh.use_auto_smooth = mesh.use_auto_smooth
                new_mesh.auto_smooth_angle = mesh.auto_smooth_angle
            
            # Copy vertex colors if they exist
            for vc in mesh.vertex_colors:
                new_vc = new_mesh.vertex_colors.new(name=vc.name)
            
            # Update mesh to ensure all data is properly initialized
            new_mesh.update()
            
            # Add to scene
            bpy.context.collection.objects.link(new_obj)
            new_objects.append(new_obj)
            
            submesh_counter += 1
            
            print(f"Created submesh {new_obj.name} with {len(new_mesh.vertices)} vertices")
    
    # Clean up
    bm.free()
    
    # Remove original object 
    bpy.data.objects.remove(obj, do_unlink=True)
    
    return new_objects

def main():
    """
    Main function to split selected objects
    """
    # Get selected objects
    selected_objects = [obj for obj in bpy.context.selected_objects if obj.type == 'MESH']
    
    if not selected_objects:
        print("No mesh objects selected!")
        return
    
    # Process each selected object
    all_new_objects = []
    for obj in selected_objects:
        print(f"\nProcessing object: {obj.name}")
        print(f"Original vertex count: {len(obj.data.vertices)}")
        
        new_objects = split_mesh_by_vertex_limit(obj, max_verts=128)
        all_new_objects.extend(new_objects)
        
        print(f"Created {len(new_objects)} submeshes")
    
    # Select all new objects
    bpy.ops.object.select_all(action='DESELECT')
    for obj in all_new_objects:
        obj.select_set(True)
    
    if all_new_objects:
        bpy.context.view_layer.objects.active = all_new_objects[0]

# Run the script
if __name__ == "__main__":
    main()