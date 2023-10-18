import sys
import matplotlib.pyplot as plt

# Function to parse data from the text file
def parse_data(file_path):
    data = []
    with open(file_path, 'r') as file:
        for line in file:
            parts = line.strip().split(',')
            event_id = int(parts[0].strip('<>'))
            event_times = [tuple(map(float, pair.strip('()').split(','))) for pair in parts[1:]]
            print(event_times)
            data.append((event_id, event_times))
    return data

# Read data from the text file
#file_path = 'link-activity.txt'  # Replace with the path to your data file
file_path = sys.argv[1]  # Replace with the path to your data file
data = parse_data(file_path)

#print( "Length of the array ", len(data))
#for a in data:
#	print("Length of each row: ", len(a), "First element: ", a[0], "second element: ", a[1])



# Create a figure and axis
fig, ax = plt.subplots(figsize=(10, 6))

# Plot timeline events
for event_id, event_times in data:
    for start, end in event_times:
        ax.plot([start, end], [event_id, event_id], linewidth=8, solid_capstyle='butt', label=f"Event {event_id}")

# Set labels and title
ax.set_xlabel("Timeline")
ax.set_ylabel("Events")
ax.set_title("Timeline Graph")

# Customize the appearance of the y-axis labels
event_ids = [event_id for event_id, _ in data]
ax.set_yticks(event_ids)
ax.set_yticklabels([f"Event {event_id}" for event_id in event_ids])

# Customize x-axis if needed (e.g., date formatting)
# ax.xaxis_date()
# ax.xaxis.set_major_formatter(plt.DateFormatter("%H:%M"))
# plt.xticks(rotation=45)

# Show legend
plt.legend()

# Show the timeline graph
plt.tight_layout()
plt.show()

