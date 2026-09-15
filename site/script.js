document.addEventListener("DOMContentLoaded", () => {
  document.querySelectorAll("[data-before-after]").forEach((comparison) => {
    const range = comparison.querySelector('input[type="range"]');
    const after = comparison.querySelector(".after-image");
    const handle = comparison.querySelector(".comparison-handle");
    const knob = comparison.querySelector(".comparison-knob");

    const update = () => {
      const value = Number(range.value);
      after.style.clipPath = `inset(0 0 0 ${value}%)`;
      handle.style.left = `${value}%`;
      knob.style.left = `${value}%`;
    };

    range.addEventListener("input", update);
    update();
  });
});
