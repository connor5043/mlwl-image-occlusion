#!/bin/bash

# Ensure required environment variables are set
if [[ -z "$COURSE_ID" || -z "$COLUMN_ID" || -z "$AUTH_TOKEN" ]]; then
  echo "Error: Please set COURSE_ID, COLUMN_ID, and AUTH_TOKEN environment variables."
  exit 1
fi

# API endpoint
API_URL="https://backend.mylittlewordland.com/api/course/$COURSE_ID/update-vocabulary"

# Function to generate a random ID
generate_random_id() {
  # Generate two smaller random numbers and concatenate them to form the ID
  local part1=$(shuf -i 100000000000-999999999999 -n 1)
  local part2=$(shuf -i 100000000000-999999999999 -n 1)
  echo "z${part1}.${part2}"
}

# Initialize addedEntries array
added_entries="["

# Process each cropped image and extract OCR text
for file in cropped_rectangle_*.png; do
  if [[ -f "$file" ]]; then
    echo "Processing $file..."
    OCR_TEXT=$(tesseract "$file" stdout -l eng --psm 6 | tr '\n' ' ')
    RANDOM_ID=$(generate_random_id)

    # Add entry to the payload
    added_entries+="{\"id\":\"$RANDOM_ID\",\"columns\":{\"$COLUMN_ID\":{\"id\":\"$COLUMN_ID\",\"values\":\"$OCR_TEXT\"}}},"
  fi
done

# Remove trailing comma from addedEntries
added_entries=${added_entries%,}
added_entries+="]"

# Construct the JSON payload
JSON_PAYLOAD=$(cat <<EOF
{
  "addedHeaders": [],
  "updatedHeaders": [],
  "deletedHeaders": [],
  "addedEntries": $added_entries,
  "updatedEntries": [],
  "deletedEntries": []
}
EOF
)

# Send the POST request
echo "Sending POST request to $API_URL..."
curl -X POST "$API_URL" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $AUTH_TOKEN" \
  -d "$JSON_PAYLOAD"

echo "Request sent."

# Count the number of cropped_rectangle_*.png files
x=$(ls cropped_rectangle_*.png 2> /dev/null | wc -l)

# Check if there are any matching files
if [[ "$x" -eq 0 ]]; then
  echo "No cropped_rectangle_*.png files found."
  exit 0
fi

echo "Number of cropped_rectangle_*.png files: $x"

# API endpoint for entries
ENTRIES_API_URL="https://backend.mylittlewordland.com/api/course/$COURSE_ID/entries"

# Fetch entries from the API
echo "Fetching entries from $ENTRIES_API_URL..."
response=$(curl -s -X GET "$ENTRIES_API_URL" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $AUTH_TOKEN")

# Check if the response is valid
if [[ -z "$response" ]]; then
  echo "Error: No response from API."
  exit 1
fi

# Extract the last x "id" values from the "entries" object
echo "Extracting the last $x 'id' values..."
ids=$(echo "$response" | jq -r --argjson x "$x" '.entries | to_entries | sort_by(.key) | .[-$x:] | .[].value.id')

# Check if any IDs were found
if [[ -z "$ids" ]]; then
  echo "Error: No 'id' values found in the 'entries' object."
  exit 1
fi

# Prepare to submit the updates
UPDATE_API_URL="https://backend.mylittlewordland.com/api/course/$COURSE_ID/update-vocabulary"

echo "Submitting updates to $UPDATE_API_URL..."

# Initialize index for file matching
index=0

# Loop over each ID and corresponding file
for id in $ids; do
  # Get the corresponding file
  filename=$(ls output_rectangle_*.png | sed -n "$((index + 1))p")
  
  # Check if the file exists
  if [[ -z "$filename" ]]; then
    echo "Error: No matching file found for index $index."
    exit 1
  fi

  # Define the column ID minus 1
  column_id_minus_1=$((COLUMN_ID - 1))

  base64 $filename | tr -d '\n' > .base64test
  echo ".png" >> .base64test
  # Construct the payload
  payload=$(cat <<EOF
{
  "updatedEntries": [
    {
      "id": "$id",
      "columns": {
        "$column_id_minus_1": {
          "id": "$column_id_minus_1",
          "values": "$(curl -s -X POST "https://backend.mylittlewordland.com/api/creator/course/$COURSE_ID/upload-image" -H "Authorization: Bearer $AUTH_TOKEN" -d @.base64test)"
        }
      }
    }
  ]
}
EOF
)

  rm .base64test

  # Submit the update
  echo "Submitting update for ID $id with file $filename..."
  curl -s -X POST "$UPDATE_API_URL" \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $AUTH_TOKEN" \
    -d "$payload"
  
  echo "Update submitted for ID $id."

  # Increment index
  index=$((index + 1))
done

echo "All updates completed."

rm cropped_rectangle_*.png
rm output_rectangle_*.png

