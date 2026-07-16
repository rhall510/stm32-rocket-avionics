import folium

def plot_zoomed_path(filename):
    # We will store coordinates here so we can center the map later
    coordinates = []
    
    # Read the file and extract coordinates
    with open(filename, 'r') as file:
        for line in file:
            parts = line.strip().split(',')
            if len(parts) == 2:
                try:
                    lon = float(parts[0])
                    lat = float(parts[1])
                    coordinates.append([lat, lon]) # Note the Lat, Lon order!
                except ValueError:
                    print(f"Skipping invalid line: {line.strip()}")
                    
    if not coordinates:
        print("No valid coordinates found!")
        return

    # 1. CENTER THE MAP: Use the first coordinate as the starting center point
    start_location = coordinates[0]

    # Create the map with new zoom settings
# Replace the previous folium.Map section with this:
    my_map = folium.Map(
        location=start_location, 
        zoom_start=18,
        max_zoom=22,
        # Point to Google's satellite tile server instead
        tiles='http://mt0.google.com/vt/lyrs=s&hl=en&x={x}&y={y}&z={z}',
        attr='Google',
        max_native_zoom=20 # Google usually handles higher native zooms
    )
    
    # Add points to the map
    for point in coordinates:
        # 2. USE CIRCLE MARKERS: Tiny dots are much better for walking paths
        folium.CircleMarker(
            location=point,
            radius=2,       # Size of the dot
            color='red',    # Outline color
            fill=True,
            fill_color='red', # Inside color
            fill_opacity=1.0,
            popup=f"Lat: {point[0]}, Lon: {point[1]}"
        ).add_to(my_map)
        
    # Optional bonus: Draw a line connecting the dots to show the actual path!
    folium.PolyLine(
        locations=coordinates,
        color='blue',
        weight=2,
        opacity=0.7
    ).add_to(my_map)
    
    output_file = 'zoomed_satellite_path.html'
    my_map.save(output_file)
    print(f"Success! Map centered on {start_location}. Open '{output_file}' to view.")

# Run the function
plot_zoomed_path('latlong.txt')