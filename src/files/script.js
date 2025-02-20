'use strict'
document.addEventListener("DOMContentLoaded", function () {
    const colorPreview = document.getElementById("color-preview");
    const buttons = document.querySelectorAll('.option-button');

    const fetchColor = (r, g, b) => {
        let queryObj = {};
        if (typeof r !== "undefined") Object.assign(queryObj, { r });
        if (typeof g !== "undefined") Object.assign(queryObj, { g });
        if (typeof b !== "undefined") Object.assign(queryObj, { b });

        let queryString = (new URLSearchParams(queryObj)).toString();
        let option = `opt=${localStorage.getItem("option") != null ? localStorage.getItem("option") : "f"}`;
        if (queryString.length > 0) {
            fetch(`/api?${queryString}&${option}`)
                .then(res => res.json())
                .then(({ r, g, b }) => {
                    const rgbColor = `rgb(${r}, ${g}, ${b})`;
                    colorPreview.style.backgroundColor = rgbColor;
                })
                .catch(err => console.error("Error fetching color:", err));
        }
    }
    buttons.forEach(button => {
        button.addEventListener('click', () => {
            const option = button.getAttribute('data-option');
            console.log(`Opción seleccionada: ${option}`);
            localStorage.setItem("option", option)
            // Enviar la opción seleccionada al ESP32 mediante la API
            fetch(`/api?option=${option}`)
                .then(response => {
                    if (response.ok) {
                        console.log(`Opción ${option} enviada al ESP32`);
                    } else {
                        console.error('Error al enviar la opción al ESP32');
                    }
                });
        });
    });

    fetchColor();

    const inputColor = document.getElementById("input-color");
    const ctx = inputColor.getContext("2d");

    const img = new Image();
    img.crossOrigin = "Anonymous";
    img.setAttribute('crossOrigin', '');
    img.addEventListener("load", () => {
        ctx.drawImage(img, 0, 0, 320, 320);
    }, false);
    img.addEventListener("error", () => {
        console.error("Error loading the image.");
    }, false);
    img.src = "./chroma.png";

    const inputColorHandler = function (e) {
        let mouseX, mouseY;

        if (e.touches && e.touches.length > 0) {
            mouseX = e.touches[0].clientX - e.touches[0].target.offsetLeft;
            mouseY = e.touches[0].clientY - e.touches[0].target.offsetTop;
        } else if (e.clientX && e.clientY) { // Manejo para eventos de mouse
            mouseX = e.clientX - e.target.offsetLeft;
            mouseY = e.clientY - e.target.offsetTop;
        } else {
            console.error("No se pudo obtener las coordenadas del evento.");
            return;
        }
        const c = ctx.getImageData(mouseX, mouseY, 1, 1).data;
        fetchColor(c[0], c[1], c[2]);
    }

    inputColor.addEventListener("touchstart", inputColorHandler);
    inputColor.addEventListener("touchmove", inputColorHandler);

    colorPreview.addEventListener("click", () => fetchColor(0, 0, 0));
    colorPreview.addEventListener("touchstart", () => fetchColor(0, 0, 0));
});
