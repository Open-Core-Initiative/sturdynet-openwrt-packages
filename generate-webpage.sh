#!/bin/bash

PUBLIC=${1:-public}

function makeLink() {
	SPACE=${1}
	shift
	for I in $*; do
		BASENAME=${I##*/}
		echo "${SPACE}<li><a href=\"${I}\">${BASENAME}</a></li>"
	done
}

cd "${PUBLIC}"

cat <<END
<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8" />
    	<meta name="viewport" content="width=device-width, initial-scale=1.0" />
	<title>Sturdynet Package Downloads</title>
	<style>
	      body {
	        font-family: "Courier New", monospace;
	        background-color: #f0f0f0;
	        color: #333;
	        display: flex;
	        flex-direction: column;
	        align-items: center;
	        min-height: 100vh;
	        margin: 0;
	      }
	
	      pre {
	        font-size: 16px;
	      }
	
	      @media (max-width: 768px) {
	        pre {
	          font-size: 12px;
	        }
	      }
	
	      .ascii-table {
	        margin-top: 5px;
	        font-size: 20px;
	        text-align: center;
	        border-collapse: collapse;
	      }
	
	      .ascii-table th,
	      .ascii-table td {
	        padding: 5px;
	        border: 1px dashed #333;
	      }
	
	      .ascii-table th {
	        background-color: #f0f0f0;
	      }
	
	      .ascii-table ul:not(:last-child) {
	        border-bottom: 1px dashed #333;
	      }
	
	      .ascii-table ul {
	        list-style-type: none; /* Remove bullets */
	        padding: 5px; /* Remove padding */
	        margin: 5px; /* Remove margins */
	      }

	      a:link { text-decoration: none; }
	    </style>
</head>
<body>
	<pre>
            _____ __                 __                 __ 
           / ___// /___  ___________/ /_  ______  ___  / /_
           \__ \/ __/ / / / ___/ __  / / / / __ \/ _ \/ __/
          ___/ / /_/ /_/ / /  / /_/ / /_/ / / / /  __/ /_  
         /____/\__/\__,_/_/   \__,_/\__, /_/ /_/\___/\__/  
                                   /____/                  
                        SturdyNet v1.0-Beta
        -----------------------------------------------------
                    Based on OpenWrt ${OPENWRT_VERSION}
               Build on <em>$(date -R)</em>
                        Target: <b>${GITHUB_REF##*/}</b>
        ----------------------------------------------------- 	
	</pre>
	<table class="ascii-table">
	      <tr>
	        <th>SDK and Build Keys</th>
			<th>Packages</th>
	      </tr>
	      <tr>
	        <td>
END

for I in releases/*/packages/*; do
	VERSION=$(echo $I | cut -d / -f 2)
	ARCH=$(echo $I | cut -d / -f 4)
	FILE="${CI_PROJECT_NAME//-/_}_${VERSION//./_}"

cat <<END
	<ul>
$(makeLink "		" ${I}/../../47bc41ad54a6601d ${I}/../../openwrt-sdk.txt ${I}/../../key-build*)
	</ul>
	</td>
	<td>
	<ul>
$(makeLink "		" ${I}/custom/*)
	</ul>
END

done

cat <<END
	</td>
    </tr>
    </table>
</body>
</html>
END
